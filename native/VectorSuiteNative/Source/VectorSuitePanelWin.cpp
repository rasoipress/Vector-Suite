// Pannello nativo di Vector Suite dentro Adobe Illustrator, versione Windows.
//
// Implementa la stessa interfaccia C di VectorSuitePanel.mm, così il resto del
// plug-in non sa su quale sistema sta girando. Anche il disegno segue lo stesso
// sistema visivo: marchio isometrico, pastiglie di icona invertite, stessa
// scala tipografica, stessi raggi, e due soli valori più i grigi che ne
// derivano.
//
// Impianto delle finestre:
//
//   host        testata fissa: marchio, ricerca, categoria, conteggio
//    └ viewport area che ritaglia e porta la barra di scorrimento
//       └ canvas contenuto alto quanto serve, spostato in verticale
//
// Far scorrere il contenuto muovendo un'unica finestra evita di riposizionare
// a mano decine di controlli figli a ogni rotellata: i figli appartengono al
// canvas e si spostano con lui.
//
// Differenza dichiarata rispetto a macOS: qui non c'è il riordino delle schede
// con Control + trascina. L'ordine è quello del catalogo.

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <string>
#include <vector>

#include "VectorSuiteCatalog.h"
#include "VectorSuitePanel.h"
#include "VectorSuiteProjection.h"
#include "../Resources/Win/icons.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

namespace {

// ---------------------------------------------------------------------------
// Sistema visivo
// ---------------------------------------------------------------------------

const int kCardHeight = 96;
const int kCardRadius = 10;
const int kTileSide = 30;
const int kTileRadius = 8;
const int kGutter = 8;
const int kMargin = 14;
const int kHeaderHeight = 118;
const int kMarkBox = 896;

struct VSTheme {
	COLORREF ink;
	COLORREF paper;
	COLORREF window;
	COLORREF muted;
	COLORREF quiet;
	COLORREF line;
};

/// macOS espone il tema con i colori dinamici; su Windows va letto e basta.
bool VSSystemIsDark()
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(
			HKEY_CURRENT_USER,
			L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
			0, KEY_READ, &key) != ERROR_SUCCESS) {
		return false;
	}
	DWORD value = 1;
	DWORD size = sizeof(value);
	DWORD type = REG_DWORD;
	const LONG status = RegQueryValueExW(key, L"AppsUseLightTheme", nullptr,
										 &type, reinterpret_cast<LPBYTE>(&value), &size);
	RegCloseKey(key);
	return status == ERROR_SUCCESS && value == 0;
}

VSTheme VSCurrentTheme()
{
	VSTheme theme;
	if (VSSystemIsDark()) {
		theme.ink = RGB(255, 255, 255);
		theme.paper = RGB(30, 30, 30);
		theme.window = RGB(19, 19, 19);
		theme.muted = RGB(162, 162, 162);
		theme.quiet = RGB(110, 110, 110);
		theme.line = RGB(58, 58, 58);
	}
	else {
		theme.ink = RGB(0, 0, 0);
		theme.paper = RGB(255, 255, 255);
		theme.window = RGB(245, 245, 245);
		theme.muted = RGB(107, 107, 107);
		theme.quiet = RGB(154, 154, 154);
		theme.line = RGB(219, 219, 219);
	}
	return theme;
}

Gdiplus::Color VSToGdi(COLORREF colour, BYTE alpha = 255)
{
	return Gdiplus::Color(alpha, GetRValue(colour), GetGValue(colour), GetBValue(colour));
}

/// Rettangolo con angoli tondi, in coordinate reali per non perdere il
/// centro del tratto.
Gdiplus::GraphicsPath* VSRoundedPath(const Gdiplus::RectF& box, float radius)
{
	Gdiplus::GraphicsPath* path = new Gdiplus::GraphicsPath();
	const float diameter = radius * 2.0f;
	path->AddArc(box.X, box.Y, diameter, diameter, 180.0f, 90.0f);
	path->AddArc(box.GetRight() - diameter, box.Y, diameter, diameter, 270.0f, 90.0f);
	path->AddArc(box.GetRight() - diameter, box.GetBottom() - diameter,
				 diameter, diameter, 0.0f, 90.0f);
	path->AddArc(box.X, box.GetBottom() - diameter, diameter, diameter, 90.0f, 90.0f);
	path->CloseFigure();
	return path;
}

// ---------------------------------------------------------------------------
// Marchio
// ---------------------------------------------------------------------------
//
// Stessa geometria di Resources/VectorSuiteLogo.svg e di VectorSuitePanel.mm,
// sulla scatola di 896 unità: cubo isometrico, fenditure fra le tre facce e,
// sul vertice centrale, le due maniglie di direzione con i pomelli vuoti.

void VSDrawMark(Gdiplus::Graphics& graphics, const Gdiplus::RectF& box,
				COLORREF foreground, COLORREF background)
{
	const float side = min(box.Width, box.Height);
	const float scale = side / static_cast<float>(kMarkBox);
	const float originX = box.X + (box.Width - side) * 0.5f;
	const float originY = box.Y + (box.Height - side) * 0.5f;

	auto point = [&](float x, float y) {
		return Gdiplus::PointF(originX + x * scale, originY + y * scale);
	};

	Gdiplus::PointF hexagon[6] = {
		point(448.0f, 79.21f),  point(767.38f, 263.61f), point(767.38f, 632.4f),
		point(448.0f, 816.79f), point(128.62f, 632.4f),  point(128.62f, 263.61f)
	};
	Gdiplus::SolidBrush solid(VSToGdi(foreground));
	graphics.FillPolygon(&solid, hexagon, 6);

	Gdiplus::Pen seams(VSToGdi(background), 30.0f * scale);
	seams.SetStartCap(Gdiplus::LineCapFlat);
	seams.SetEndCap(Gdiplus::LineCapFlat);
	const Gdiplus::PointF centre = point(448.0f, 448.0f);
	graphics.DrawLine(&seams, centre, point(767.38f, 263.61f));
	graphics.DrawLine(&seams, centre, point(128.62f, 263.61f));
	graphics.DrawLine(&seams, centre, point(448.0f, 816.79f));

	const Gdiplus::PointF knobs[2] = { point(279.43f, 545.32f), point(616.57f, 545.32f) };
	Gdiplus::Pen handles(VSToGdi(background), 20.0f * scale);
	handles.SetStartCap(Gdiplus::LineCapFlat);
	handles.SetEndCap(Gdiplus::LineCapFlat);
	handles.SetLineJoin(Gdiplus::LineJoinMiter);
	graphics.DrawLine(&handles, knobs[0], centre);
	graphics.DrawLine(&handles, centre, knobs[1]);

	// I pomelli sono vuoti: pieni nel colore della sagoma, cerchiati nel colore
	// di fondo. È così che Illustrator disegna le maniglie di direzione.
	const float knob = 43.03f * scale;
	Gdiplus::Pen ring(VSToGdi(background), 20.0f * scale);
	for (int index = 0; index < 2; ++index) {
		const Gdiplus::RectF disc(knobs[index].X - knob, knobs[index].Y - knob,
								  knob * 2.0f, knob * 2.0f);
		graphics.FillEllipse(&solid, disc);
		graphics.DrawEllipse(&ring, disc);
	}
}

// ---------------------------------------------------------------------------
// Icone
// ---------------------------------------------------------------------------

int VSResourceForKey(const char* key)
{
	for (int index = 0; index < kVSWindowsIconCount; ++index) {
		if (std::strcmp(kVSWindowsIcons[index].key, key) == 0) {
			return kVSWindowsIcons[index].resource;
		}
	}
	return 0;
}

HMODULE VSModuleHandle()
{
	HMODULE module = nullptr;
	GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&VSModuleHandle),
		&module);
	return module;
}

/// I PNG delle icone sono incorporati nel binario: GDI+ li legge da uno stream
/// costruito sulla risorsa.
Gdiplus::Image* VSLoadIcon(int resourceID)
{
	HMODULE module = VSModuleHandle();
	HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceID), L"PNG");
	if (!resource) return nullptr;
	const DWORD size = SizeofResource(module, resource);
	HGLOBAL loaded = LoadResource(module, resource);
	if (!loaded || size == 0) return nullptr;
	const void* bytes = LockResource(loaded);
	if (!bytes) return nullptr;

	HGLOBAL buffer = GlobalAlloc(GMEM_MOVEABLE, size);
	if (!buffer) return nullptr;
	void* target = GlobalLock(buffer);
	if (!target) {
		GlobalFree(buffer);
		return nullptr;
	}
	std::memcpy(target, bytes, size);
	GlobalUnlock(buffer);

	IStream* stream = nullptr;
	if (CreateStreamOnHGlobal(buffer, TRUE, &stream) != S_OK) {
		GlobalFree(buffer);
		return nullptr;
	}
	Gdiplus::Image* image = Gdiplus::Image::FromStream(stream);
	stream->Release();
	if (image && image->GetLastStatus() != Gdiplus::Ok) {
		delete image;
		return nullptr;
	}
	return image;
}

/// I PNG sono bianchi: qui vengono rimappati sul colore del glifo lasciando
/// intatto il canale alfa, che è dove sta il disegno.
void VSDrawTintedIcon(Gdiplus::Graphics& graphics, Gdiplus::Image* image,
					  const Gdiplus::RectF& box, COLORREF colour)
{
	if (!image) return;
	const float red = GetRValue(colour) / 255.0f;
	const float green = GetGValue(colour) / 255.0f;
	const float blue = GetBValue(colour) / 255.0f;

	Gdiplus::ColorMatrix matrix = {
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		red,  green, blue, 0.0f, 1.0f
	};
	Gdiplus::ImageAttributes attributes;
	attributes.SetColorMatrix(&matrix);
	graphics.DrawImage(
		image, Gdiplus::RectF(box.X, box.Y, box.Width, box.Height),
		0.0f, 0.0f,
		static_cast<Gdiplus::REAL>(image->GetWidth()),
		static_cast<Gdiplus::REAL>(image->GetHeight()),
		Gdiplus::UnitPixel, &attributes);
}

// ---------------------------------------------------------------------------
// Stato del pannello
// ---------------------------------------------------------------------------

const wchar_t* kHostClass = L"VectorSuitePanelHost";
const wchar_t* kViewportClass = L"VectorSuitePanelViewport";
const wchar_t* kCanvasClass = L"VectorSuitePanelCanvas";

enum {
	kIDSearch = 3000,
	kIDCategory,
	kIDGenerate,
	kIDNewSeed,
	kIDReset,
	kIDAutomatic,
	kIDAdvanced,
	kIDGroup,
	kIDReplace,
	kIDWaveFirst,          // quattro pulsanti consecutivi
	kIDSliderFirst = 3100, // dodici cursori consecutivi
	kIDValueFirst = 3200,  // dodici caselle numeriche consecutive
	kIDProjPresetFirst = 3300, // tre preset consecutivi
	kIDProjPlaneFirst = 3310,  // tre piani consecutivi
	kIDProjSnap = 3320,
	kIDProjLeft = 3321,
	kIDProjRight = 3322
};

struct VSParameter {
	const char* key;
	const wchar_t* label;
	double minimum;
	double maximum;
	double advancedMaximum;   // 0 quando non cambia in modalità avanzata
	double defaultValue;
	int decimals;
};

// Stessi dodici parametri, stessi limiti e stessi valori di partenza del
// pannello macOS: il generatore è lo stesso, i controlli devono coincidere.
const VSParameter kParameters[] = {
	{"seed",              L"Seme",                  1,    9999,     0,     999,  0},
	{"initialLength",     L"Lunghezza iniziale",    10,   600,      0,     150,  0},
	{"lengthFalloff",     L"Decadimento lunghezza", 0.02, 0.6,      0,     0.25, 2},
	{"initialAngle",      L"Angolo iniziale",       1,    90,       0,     25,   0},
	{"angleFalloff",      L"Decadimento angolo",    0.7,  1.4,      0,     1,    2},
	{"minimumLength",     L"Lunghezza minima",      0.5,  40,       0,     2,    1},
	{"maximumIterations", L"Iterazioni max",        1,    13,       20,    11,   0},
	{"maximumPaths",      L"Tracciati max",         10,   5000,     30000, 3000, 0},
	{"initialWidth",      L"Spessore iniziale",     0.1,  200,      0,     40,   1},
	{"widthFalloff",      L"Decadimento spessore",  0.3,  1,        0,     0.72, 2},
	{"lengthRandomness",  L"Casualità lunghezza",   0,    0.8,      0,     0.15, 2},
	{"angleRandomness",   L"Casualità angolo",      0,    0.8,      0,     0.15, 2}
};
const int kParameterCount = static_cast<int>(sizeof(kParameters) / sizeof(kParameters[0]));
const int kSliderRange = 1000;   // i cursori Win32 sono interi: si mappa

struct VSCardHit {
	int moduleID;
	RECT bounds;
};

struct VSPanel {
	HWND parent = nullptr;
	HWND host = nullptr;
	HWND viewport = nullptr;
	HWND canvas = nullptr;

	VSPanelActivateToolProc activateTool = nullptr;
	VSPanelGenerateFractalProc generateFractal = nullptr;
	void* context = nullptr;

	HWND search = nullptr;
	HWND category = nullptr;
	HWND sliders[kParameterCount] = {};
	HWND values[kParameterCount] = {};
	HWND wave[4] = {};
	HWND automatic = nullptr;
	HWND advanced = nullptr;
	HWND group = nullptr;
	HWND replace = nullptr;
	HWND generate = nullptr;
	HWND newSeed = nullptr;
	HWND reset = nullptr;

	// Projection Studio
	HWND projPreset[3] = {};
	HWND projPlane[3] = {};
	HWND projSnap = nullptr;
	HWND projLeft = nullptr;
	HWND projRight = nullptr;

	HFONT fontTitle = nullptr;
	HFONT fontCard = nullptr;
	HFONT fontBody = nullptr;
	HFONT fontSmall = nullptr;
	HFONT fontEyebrow = nullptr;
	HFONT fontMono = nullptr;

	HBRUSH fieldBrush = nullptr;   // riusato: crearlo a ogni WM_CTLCOLOR perde oggetti GDI
	ULONG_PTR gdiplusToken = 0;
	std::vector<Gdiplus::Image*> icons;   // indicizzate per modulo
	std::vector<VSCardHit> cards;
	std::vector<int> visibleModules;

	int selectedModule = kVSSuiteCore;
	int dpi = 96;
	int contentHeight = 0;
	int scrollY = 0;
	std::wstring status = L"Pronto";
	VSTheme theme;
};

int VSScale(const VSPanel* panel, int value)
{
	return MulDiv(value, panel->dpi, 96);
}

std::wstring VSWiden(const char* text)
{
	if (!text) return std::wstring();
	const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
	if (length <= 0) return std::wstring();
	std::wstring result(static_cast<size_t>(length - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text, -1, &result[0], length);
	return result;
}

// ---------------------------------------------------------------------------
// Preferenze
// ---------------------------------------------------------------------------
//
// L'equivalente di NSUserDefaults: una chiave sotto HKCU, così le impostazioni
// di Fractal Grove sopravvivono alla chiusura di Illustrator.

const wchar_t* kPreferencesKey = L"Software\\Vector Suite\\Fractal";

double VSReadPreference(const char* key, double fallback)
{
	HKEY handle = nullptr;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, kPreferencesKey, 0, KEY_READ, &handle)
			!= ERROR_SUCCESS) {
		return fallback;
	}
	wchar_t buffer[64] = {};
	DWORD size = sizeof(buffer);
	DWORD type = REG_SZ;
	const std::wstring name = VSWiden(key);
	const LONG status = RegQueryValueExW(handle, name.c_str(), nullptr, &type,
										 reinterpret_cast<LPBYTE>(buffer), &size);
	RegCloseKey(handle);
	if (status != ERROR_SUCCESS) return fallback;
	return _wtof(buffer);
}

void VSWritePreference(const char* key, double value)
{
	HKEY handle = nullptr;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, kPreferencesKey, 0, nullptr, 0,
						KEY_WRITE, nullptr, &handle, nullptr) != ERROR_SUCCESS) {
		return;
	}
	wchar_t buffer[64];
	swprintf_s(buffer, L"%.6f", value);
	const std::wstring name = VSWiden(key);
	RegSetValueExW(handle, name.c_str(), 0, REG_SZ,
				   reinterpret_cast<const BYTE*>(buffer),
				   static_cast<DWORD>((wcslen(buffer) + 1) * sizeof(wchar_t)));
	RegCloseKey(handle);
}

// ---------------------------------------------------------------------------
// Parametri di Fractal Grove
// ---------------------------------------------------------------------------

const wchar_t* kProjectionKey = L"Software\\Vector Suite\\Projection";

double VSReadProjection(const char* key, double fallback)
{
	HKEY handle = nullptr;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, kProjectionKey, 0, KEY_READ, &handle)
			!= ERROR_SUCCESS) {
		return fallback;
	}
	wchar_t buffer[64] = {};
	DWORD size = sizeof(buffer);
	DWORD type = REG_SZ;
	const std::wstring name = VSWiden(key);
	const LONG status = RegQueryValueExW(handle, name.c_str(), nullptr, &type,
										 reinterpret_cast<LPBYTE>(buffer), &size);
	RegCloseKey(handle);
	return status == ERROR_SUCCESS ? _wtof(buffer) : fallback;
}

void VSWriteProjection(const char* key, double value)
{
	HKEY handle = nullptr;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, kProjectionKey, 0, nullptr, 0,
						KEY_WRITE, nullptr, &handle, nullptr) != ERROR_SUCCESS) {
		return;
	}
	wchar_t buffer[64];
	swprintf_s(buffer, L"%.6f", value);
	const std::wstring name = VSWiden(key);
	RegSetValueExW(handle, name.c_str(), 0, REG_SZ,
				   reinterpret_cast<const BYTE*>(buffer),
				   static_cast<DWORD>((wcslen(buffer) + 1) * sizeof(wchar_t)));
	RegCloseKey(handle);
}

/// Ripristina le impostazioni salvate e le consegna al plug-in.
void VSRestoreProjection()
{
	VSProjectionSettings settings = VSProjectionDefaults();
	settings.leftAngle = VSReadProjection("leftAngle", settings.leftAngle);
	settings.rightAngle = VSReadProjection("rightAngle", settings.rightAngle);
	settings.plane = static_cast<int>(VSReadProjection("plane", settings.plane));
	settings.snapLine = VSReadProjection("snapLine", settings.snapLine) != 0 ? 1 : 0;
	settings.moveDistance = VSReadProjection("moveDistance", settings.moveDistance);
	settings.moveAxis = static_cast<int>(VSReadProjection("moveAxis", settings.moveAxis));
	settings.scaleU = VSReadProjection("scaleU", settings.scaleU);
	settings.scaleV = VSReadProjection("scaleV", settings.scaleV);
	settings.rotation = VSReadProjection("rotation", settings.rotation);
	settings.shear = VSReadProjection("shear", settings.shear);
	VSProjectionSet(&settings);
}

void VSStoreProjection(const VSProjectionSettings& settings)
{
	VSProjectionSet(&settings);
	VSWriteProjection("leftAngle", settings.leftAngle);
	VSWriteProjection("rightAngle", settings.rightAngle);
	VSWriteProjection("plane", settings.plane);
	VSWriteProjection("snapLine", settings.snapLine);
	VSWriteProjection("moveDistance", settings.moveDistance);
	VSWriteProjection("moveAxis", settings.moveAxis);
	VSWriteProjection("scaleU", settings.scaleU);
	VSWriteProjection("scaleV", settings.scaleV);
	VSWriteProjection("rotation", settings.rotation);
	VSWriteProjection("shear", settings.shear);
}

bool VSAdvancedOn(const VSPanel* panel)
{
	return panel->advanced && SendMessageW(panel->advanced, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

double VSMaximumFor(const VSPanel* panel, int index)
{
	const VSParameter& parameter = kParameters[index];
	if (parameter.advancedMaximum > 0.0 && VSAdvancedOn(panel)) {
		return parameter.advancedMaximum;
	}
	return parameter.maximum;
}

double VSValueFor(const VSPanel* panel, int index)
{
	const VSParameter& parameter = kParameters[index];
	const double maximum = VSMaximumFor(panel, index);
	if (!panel->sliders[index]) {
		double stored = VSReadPreference(parameter.key, parameter.defaultValue);
		return max(parameter.minimum, min(maximum, stored));
	}
	const LRESULT position = SendMessageW(panel->sliders[index], TBM_GETPOS, 0, 0);
	const double ratio = static_cast<double>(position) / kSliderRange;
	return parameter.minimum + ratio * (maximum - parameter.minimum);
}

void VSFormatValue(const VSParameter& parameter, double value, wchar_t* buffer, size_t count)
{
	if (parameter.decimals == 0) swprintf_s(buffer, count, L"%.0f", value);
	else if (parameter.decimals == 1) swprintf_s(buffer, count, L"%.1f", value);
	else swprintf_s(buffer, count, L"%.2f", value);
}

void VSSyncValueField(VSPanel* panel, int index)
{
	if (!panel->values[index]) return;
	wchar_t buffer[64];
	VSFormatValue(kParameters[index], VSValueFor(panel, index), buffer, 64);
	SetWindowTextW(panel->values[index], buffer);
}

void VSSetSlider(VSPanel* panel, int index, double value)
{
	const VSParameter& parameter = kParameters[index];
	const double maximum = VSMaximumFor(panel, index);
	const double clamped = max(parameter.minimum, min(maximum, value));
	const double span = maximum - parameter.minimum;
	const int position = span > 0.0
		? static_cast<int>(std::lround((clamped - parameter.minimum) / span * kSliderRange))
		: 0;
	if (panel->sliders[index]) {
		SendMessageW(panel->sliders[index], TBM_SETPOS, TRUE, position);
	}
	VSWritePreference(parameter.key, clamped);
	VSSyncValueField(panel, index);
}

int VSWaveSelection(const VSPanel* panel)
{
	for (int index = 0; index < 4; ++index) {
		if (panel->wave[index] &&
			SendMessageW(panel->wave[index], BM_GETCHECK, 0, 0) == BST_CHECKED) {
			return index;
		}
	}
	return 1;
}

VSFractalSettings VSCurrentSettings(VSPanel* panel)
{
	VSFractalSettings settings = {};
	settings.seed = VSValueFor(panel, 0);
	settings.initialLength = VSValueFor(panel, 1);
	settings.lengthFalloff = VSValueFor(panel, 2);
	settings.initialAngle = VSValueFor(panel, 3);
	settings.angleFalloff = VSValueFor(panel, 4);
	settings.minimumLength = VSValueFor(panel, 5);
	settings.maximumIterations = static_cast<int>(std::lround(VSValueFor(panel, 6)));
	settings.maximumPaths = static_cast<int>(std::lround(VSValueFor(panel, 7)));
	settings.initialWidth = VSValueFor(panel, 8);
	settings.widthFalloff = VSValueFor(panel, 9);
	settings.lengthRandomness = VSValueFor(panel, 10);
	settings.angleRandomness = VSValueFor(panel, 11);
	settings.wave = VSWaveSelection(panel);
	settings.automatic = panel->automatic &&
		SendMessageW(panel->automatic, BM_GETCHECK, 0, 0) == BST_CHECKED;
	settings.advanced = VSAdvancedOn(panel);
	settings.group = panel->group &&
		SendMessageW(panel->group, BM_GETCHECK, 0, 0) == BST_CHECKED;
	settings.replace = panel->replace &&
		SendMessageW(panel->replace, BM_GETCHECK, 0, 0) == BST_CHECKED;
	return settings;
}

void VSGenerateNow(VSPanel* panel)
{
	if (!panel->generateFractal) return;
	panel->status = L"Generazione…";
	InvalidateRect(panel->canvas, nullptr, FALSE);
	UpdateWindow(panel->canvas);

	VSFractalSettings settings = VSCurrentSettings(panel);
	const int count = panel->generateFractal(panel->context, &settings);
	wchar_t buffer[128];
	if (count >= 0) {
		swprintf_s(buffer, L"%d tracciati · seme %.0f", count, settings.seed);
		panel->status = buffer;
	}
	else {
		panel->status = L"Generazione non riuscita";
	}
	InvalidateRect(panel->canvas, nullptr, FALSE);
}

bool VSAutomaticOn(const VSPanel* panel)
{
	return panel->automatic &&
		SendMessageW(panel->automatic, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

// ---------------------------------------------------------------------------
// Filtro dei moduli
// ---------------------------------------------------------------------------

std::wstring VSSearchText(const VSPanel* panel)
{
	if (!panel->search) return std::wstring();
	wchar_t buffer[256] = {};
	GetWindowTextW(panel->search, buffer, 256);
	return std::wstring(buffer);
}

bool VSMatches(const VSPanel* panel, const VSModuleDefinition& module)
{
	const int selection = panel->category
		? static_cast<int>(SendMessageW(panel->category, CB_GETCURSEL, 0, 0))
		: 0;
	static const char* kCategories[] = {
		"", "Disegno", "Geometria", "Aspetto", "Workflow", "Sistema"
	};
	if (selection > 0 && selection < 6 &&
		std::strcmp(module.category, kCategories[selection]) != 0) {
		return false;
	}

	std::wstring query = VSSearchText(panel);
	if (query.empty()) return true;
	for (size_t index = 0; index < query.size(); ++index) {
		query[index] = towlower(query[index]);
	}

	std::wstring haystack = VSWiden(module.name) + L" " + VSWiden(module.summary) +
		L" " + VSWiden(module.category);
	for (size_t index = 0; index < haystack.size(); ++index) {
		haystack[index] = towlower(haystack[index]);
	}
	return haystack.find(query) != std::wstring::npos;
}

void VSRefreshVisible(VSPanel* panel)
{
	panel->visibleModules.clear();
	for (int moduleID = 0; moduleID < kVSModuleCount; ++moduleID) {
		if (VSMatches(panel, kVSModules[moduleID])) {
			panel->visibleModules.push_back(moduleID);
		}
	}
}

// ---------------------------------------------------------------------------
// Disposizione
// ---------------------------------------------------------------------------

void VSShowProjectionControls(VSPanel* panel, bool visible)
{
	const int mode = visible ? SW_SHOW : SW_HIDE;
	for (int index = 0; index < 3; ++index) {
		if (panel->projPreset[index]) ShowWindow(panel->projPreset[index], mode);
		if (panel->projPlane[index]) ShowWindow(panel->projPlane[index], mode);
	}
	if (panel->projLeft) ShowWindow(panel->projLeft, mode);
	if (panel->projRight) ShowWindow(panel->projRight, mode);
	if (panel->projSnap) ShowWindow(panel->projSnap, mode);
}

void VSShowFractalControls(VSPanel* panel, bool visible)
{
	const int mode = visible ? SW_SHOW : SW_HIDE;
	for (int index = 0; index < kParameterCount; ++index) {
		if (panel->sliders[index]) ShowWindow(panel->sliders[index], mode);
		if (panel->values[index]) ShowWindow(panel->values[index], mode);
	}
	for (int index = 0; index < 4; ++index) {
		if (panel->wave[index]) ShowWindow(panel->wave[index], mode);
	}
	HWND rest[] = { panel->automatic, panel->advanced, panel->group,
					panel->replace, panel->generate, panel->newSeed, panel->reset };
	for (int index = 0; index < 7; ++index) {
		if (rest[index]) ShowWindow(rest[index], mode);
	}
}

/// Calcola la posizione di schede e controlli e restituisce l'altezza totale
/// del contenuto. Il canvas viene poi ridimensionato di conseguenza.
int VSLayoutCanvas(VSPanel* panel)
{
	RECT client;
	GetClientRect(panel->viewport, &client);
	const int width = client.right - client.left;
	const int margin = VSScale(panel, kGutter);
	const int gutter = VSScale(panel, kGutter);
	const int cardHeight = VSScale(panel, kCardHeight);
	const int columnWidth = (width - margin * 2 - gutter) / 2;

	panel->cards.clear();
	int y = margin;
	for (size_t index = 0; index < panel->visibleModules.size(); index += 2) {
		for (int column = 0; column < 2; ++column) {
			const size_t position = index + column;
			if (position >= panel->visibleModules.size()) break;
			VSCardHit hit;
			hit.moduleID = panel->visibleModules[position];
			hit.bounds.left = margin + column * (columnWidth + gutter);
			hit.bounds.top = y;
			hit.bounds.right = hit.bounds.left + columnWidth;
			hit.bounds.bottom = y + cardHeight;
			panel->cards.push_back(hit);
		}
		y += cardHeight + gutter;
	}

	const int labelColumn = VSScale(panel, 112);
	const int gap = VSScale(panel, 8);
	const int contentLeft = margin + VSScale(panel, 4);
	const int contentRight = width - margin - VSScale(panel, 4);

	const bool showProjection = panel->selectedModule == kVSProjectionStudio;
	VSShowProjectionControls(panel, showProjection);
	if (showProjection) {
		y += VSScale(panel, 8) + VSScale(panel, 20);   // intestazione «Proiezione»
		const int third = (contentRight - contentLeft) / 3;
		for (int index = 0; index < 3; ++index) {
			MoveWindow(panel->projPreset[index], contentLeft + index * third, y,
					   third, VSScale(panel, 22), TRUE);
		}
		y += VSScale(panel, 28);

		const int sliderWidth = contentRight - contentLeft - labelColumn - gap -
								VSScale(panel, 56) - gap;
		MoveWindow(panel->projLeft, contentLeft + labelColumn + gap, y,
				   sliderWidth, VSScale(panel, 20), TRUE);
		y += VSScale(panel, 24);
		MoveWindow(panel->projRight, contentLeft + labelColumn + gap, y,
				   sliderWidth, VSScale(panel, 20), TRUE);
		y += VSScale(panel, 24);

		y += VSScale(panel, 20);                        // intestazione «Piano attivo»
		for (int index = 0; index < 3; ++index) {
			MoveWindow(panel->projPlane[index], contentLeft + index * third, y,
					   third, VSScale(panel, 22), TRUE);
		}
		y += VSScale(panel, 28);

		MoveWindow(panel->projSnap, contentLeft, y,
				   contentRight - contentLeft, VSScale(panel, 22), TRUE);
		y += VSScale(panel, 30);
		return y + margin;
	}

	const bool showFractal = panel->selectedModule == kVSFractalGrove;
	VSShowFractalControls(panel, showFractal);
	if (!showFractal) {
		return y + margin;
	}

	// Blocco di Fractal Grove: etichetta, cursore, casella numerica.
	const int rowHeight = VSScale(panel, 24);
	const int labelWidth = VSScale(panel, 112);
	const int valueWidth = VSScale(panel, 56);
	const int spacing = VSScale(panel, 8);
	const int left = margin + VSScale(panel, 4);
	const int right = width - margin - VSScale(panel, 4);
	const int sliderWidth = right - left - labelWidth - valueWidth - spacing * 2;

	y += VSScale(panel, 8);
	for (int index = 0; index < kParameterCount; ++index) {
		// Le due intestazioni di sezione occupano una riga ciascuna.
		if (index == 0 || index == 8) y += VSScale(panel, 20);

		const int sliderX = left + labelWidth + spacing;
		if (panel->sliders[index]) {
			MoveWindow(panel->sliders[index], sliderX, y, sliderWidth,
					   VSScale(panel, 20), TRUE);
		}
		if (panel->values[index]) {
			MoveWindow(panel->values[index], sliderX + sliderWidth + spacing,
					   y, valueWidth, VSScale(panel, 20), TRUE);
		}
		y += rowHeight;
	}

	y += VSScale(panel, 20);                       // intestazione "Ondulazione"
	const int waveWidth = (right - left) / 4;
	for (int index = 0; index < 4; ++index) {
		if (panel->wave[index]) {
			MoveWindow(panel->wave[index], left + index * waveWidth, y,
					   waveWidth, VSScale(panel, 22), TRUE);
		}
	}
	y += VSScale(panel, 30);

	y += VSScale(panel, 20);                       // intestazione "Generazione"
	const int halfWidth = (right - left - spacing) / 2;
	HWND row1[2] = { panel->automatic, panel->advanced };
	HWND row2[2] = { panel->group, panel->replace };
	for (int index = 0; index < 2; ++index) {
		if (row1[index]) {
			MoveWindow(row1[index], left + index * (halfWidth + spacing), y,
					   halfWidth, VSScale(panel, 22), TRUE);
		}
	}
	y += VSScale(panel, 26);
	for (int index = 0; index < 2; ++index) {
		if (row2[index]) {
			MoveWindow(row2[index], left + index * (halfWidth + spacing), y,
					   halfWidth, VSScale(panel, 22), TRUE);
		}
	}
	y += VSScale(panel, 32);

	if (panel->newSeed) {
		MoveWindow(panel->newSeed, left, y, halfWidth, VSScale(panel, 24), TRUE);
	}
	if (panel->reset) {
		MoveWindow(panel->reset, left + halfWidth + spacing, y, halfWidth,
				   VSScale(panel, 24), TRUE);
	}
	y += VSScale(panel, 32);

	if (panel->generate) {
		MoveWindow(panel->generate, left, y, right - left, VSScale(panel, 30), TRUE);
	}
	y += VSScale(panel, 38);
	y += VSScale(panel, 22);                       // riga di stato

	return y + margin;
}

void VSUpdateScroll(VSPanel* panel)
{
	RECT client;
	GetClientRect(panel->viewport, &client);
	const int visibleHeight = client.bottom - client.top;

	SCROLLINFO info = {};
	info.cbSize = sizeof(info);
	info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	info.nMin = 0;
	info.nMax = max(0, panel->contentHeight - 1);
	info.nPage = static_cast<UINT>(max(1, visibleHeight));
	info.nPos = panel->scrollY;
	SetScrollInfo(panel->viewport, SB_VERT, &info, TRUE);

	const int maximumScroll = max(0, panel->contentHeight - visibleHeight);
	if (panel->scrollY > maximumScroll) panel->scrollY = maximumScroll;

	MoveWindow(panel->canvas, 0, -panel->scrollY,
			   client.right - client.left, max(panel->contentHeight, visibleHeight), TRUE);
}

void VSRelayout(VSPanel* panel)
{
	if (!panel->viewport || !panel->canvas) return;
	VSRefreshVisible(panel);
	panel->contentHeight = VSLayoutCanvas(panel);
	VSUpdateScroll(panel);
	InvalidateRect(panel->canvas, nullptr, TRUE);
	InvalidateRect(panel->host, nullptr, TRUE);
}

// ---------------------------------------------------------------------------
// Disegno
// ---------------------------------------------------------------------------

void VSDrawText(HDC device, const std::wstring& text, RECT bounds, HFONT font,
				COLORREF colour, UINT format)
{
	HFONT previous = static_cast<HFONT>(SelectObject(device, font));
	SetBkMode(device, TRANSPARENT);
	SetTextColor(device, colour);
	DrawTextW(device, text.c_str(), -1, &bounds, format);
	SelectObject(device, previous);
}

void VSPaintHeader(VSPanel* panel, HDC device, const RECT& client)
{
	HBRUSH background = CreateSolidBrush(panel->theme.window);
	FillRect(device, &client, background);
	DeleteObject(background);

	Gdiplus::Graphics graphics(device);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

	const int margin = VSScale(panel, kMargin);
	const int tile = VSScale(panel, 32);

	Gdiplus::RectF plate(static_cast<Gdiplus::REAL>(margin),
						 static_cast<Gdiplus::REAL>(margin),
						 static_cast<Gdiplus::REAL>(tile),
						 static_cast<Gdiplus::REAL>(tile));
	Gdiplus::GraphicsPath* rounded = VSRoundedPath(plate, VSScale(panel, 9) * 1.0f);
	Gdiplus::SolidBrush inkBrush(VSToGdi(panel->theme.ink));
	graphics.FillPath(&inkBrush, rounded);
	delete rounded;

	const Gdiplus::REAL inset = tile * 0.10f;
	VSDrawMark(graphics,
			   Gdiplus::RectF(plate.X + inset, plate.Y + inset,
							  plate.Width - inset * 2, plate.Height - inset * 2),
			   panel->theme.paper, panel->theme.ink);

	RECT title = { margin + tile + VSScale(panel, 10), margin,
				   client.right - margin, margin + VSScale(panel, 18) };
	VSDrawText(device, L"Vector Suite", title, panel->fontTitle,
			   panel->theme.ink, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

	RECT subtitle = { title.left, title.bottom, client.right - margin,
					  title.bottom + VSScale(panel, 15) };
	VSDrawText(device, L"Adobe Illustrator 2026", subtitle, panel->fontSmall,
			   panel->theme.muted, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

	const int metaTop = VSScale(panel, kHeaderHeight) - VSScale(panel, 20);
	RECT eyebrow = { margin, metaTop, client.right / 2, metaTop + VSScale(panel, 16) };
	VSDrawText(device, L"LIBRERIA", eyebrow, panel->fontEyebrow,
			   panel->theme.quiet, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

	wchar_t count[64];
	const size_t visible = panel->visibleModules.size();
	swprintf_s(count, visible == 1 ? L"%zu modulo" : L"%zu moduli", visible);
	RECT counter = { client.right / 2, metaTop, client.right - margin,
					 metaTop + VSScale(panel, 16) };
	VSDrawText(device, count, counter, panel->fontSmall, panel->theme.quiet,
			   DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

	HPEN pen = CreatePen(PS_SOLID, 1, panel->theme.line);
	HPEN previousPen = static_cast<HPEN>(SelectObject(device, pen));
	MoveToEx(device, 0, client.bottom - 1, nullptr);
	LineTo(device, client.right, client.bottom - 1);
	SelectObject(device, previousPen);
	DeleteObject(pen);
}

void VSPaintCards(VSPanel* panel, HDC device, const RECT& client)
{
	HBRUSH background = CreateSolidBrush(panel->theme.window);
	FillRect(device, &client, background);
	DeleteObject(background);

	Gdiplus::Graphics graphics(device);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

	Gdiplus::SolidBrush paperBrush(VSToGdi(panel->theme.paper));
	Gdiplus::SolidBrush inkBrush(VSToGdi(panel->theme.ink));
	Gdiplus::Pen linePen(VSToGdi(panel->theme.line), 1.0f);
	Gdiplus::Pen selectedPen(VSToGdi(panel->theme.ink), 1.5f);

	const float radius = static_cast<float>(VSScale(panel, kCardRadius));
	const int tileSide = VSScale(panel, kTileSide);
	const int padding = VSScale(panel, 12);

	for (size_t index = 0; index < panel->cards.size(); ++index) {
		const VSCardHit& card = panel->cards[index];
		const VSModuleDefinition& module = kVSModules[card.moduleID];
		const bool selected = card.moduleID == panel->selectedModule;

		Gdiplus::RectF box(
			static_cast<Gdiplus::REAL>(card.bounds.left) + 0.5f,
			static_cast<Gdiplus::REAL>(card.bounds.top) + 0.5f,
			static_cast<Gdiplus::REAL>(card.bounds.right - card.bounds.left) - 1.0f,
			static_cast<Gdiplus::REAL>(card.bounds.bottom - card.bounds.top) - 1.0f);
		Gdiplus::GraphicsPath* shape = VSRoundedPath(box, radius);
		graphics.FillPath(&paperBrush, shape);
		graphics.DrawPath(selected ? &selectedPen : &linePen, shape);
		delete shape;

		Gdiplus::RectF plate(box.X + padding, box.Y + padding,
							 static_cast<Gdiplus::REAL>(tileSide),
							 static_cast<Gdiplus::REAL>(tileSide));
		Gdiplus::GraphicsPath* rounded =
			VSRoundedPath(plate, static_cast<float>(VSScale(panel, kTileRadius)));
		graphics.FillPath(&inkBrush, rounded);
		delete rounded;

		if (card.moduleID < static_cast<int>(panel->icons.size())) {
			const Gdiplus::REAL glyphInset = tileSide * 0.17f;
			VSDrawTintedIcon(graphics, panel->icons[card.moduleID],
							 Gdiplus::RectF(plate.X + glyphInset, plate.Y + glyphInset,
											plate.Width - glyphInset * 2,
											plate.Height - glyphInset * 2),
							 panel->theme.paper);
		}

		RECT name = { card.bounds.left + padding,
					  card.bounds.top + padding + tileSide + VSScale(panel, 8),
					  card.bounds.right - padding,
					  card.bounds.top + padding + tileSide + VSScale(panel, 24) };
		VSDrawText(device, VSWiden(module.name), name, panel->fontCard,
				   panel->theme.ink, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

		RECT summary = { name.left, name.bottom, name.right,
						 card.bounds.bottom - VSScale(panel, 6) };
		VSDrawText(device, VSWiden(module.summary), summary, panel->fontSmall,
				   panel->theme.muted, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
	}

	if (panel->selectedModule == kVSProjectionStudio && !panel->cards.empty()) {
		const int left = VSScale(panel, kGutter) + VSScale(panel, 4);
		int y = panel->cards.back().bounds.bottom + VSScale(panel, kGutter) + VSScale(panel, 8);
		const VSProjectionSettings settings = VSProjectionGet();

		RECT head = { left, y, client.right - left, y + VSScale(panel, 18) };
		VSDrawText(device, L"PROIEZIONE", head, panel->fontEyebrow,
				   panel->theme.quiet, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
		y += VSScale(panel, 20) + VSScale(panel, 28);

		const wchar_t* captions[2] = { L"Angolo sinistro", L"Angolo destro" };
		const double angles[2] = { settings.leftAngle, settings.rightAngle };
		for (int index = 0; index < 2; ++index) {
			RECT caption = { left, y, left + VSScale(panel, 112), y + VSScale(panel, 20) };
			VSDrawText(device, captions[index], caption, panel->fontBody,
					   panel->theme.muted, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
			wchar_t value[32];
			swprintf_s(value, L"%.0f°", angles[index]);
			RECT box = { client.right - left - VSScale(panel, 56), y,
						 client.right - left, y + VSScale(panel, 20) };
			VSDrawText(device, value, box, panel->fontMono ? panel->fontMono : panel->fontBody,
					   panel->theme.ink, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
			y += VSScale(panel, 24);
		}

		RECT plane = { left, y, client.right - left, y + VSScale(panel, 18) };
		VSDrawText(device, L"PIANO ATTIVO", plane, panel->fontEyebrow,
				   panel->theme.quiet, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
		return;
	}

	if (panel->selectedModule != kVSFractalGrove || panel->cards.empty()) return;

	// Intestazioni di sezione e riga di stato di Fractal Grove: i controlli sono
	// finestre figlie, queste sono solo scritte sul canvas.
	const int margin = VSScale(panel, kGutter) + VSScale(panel, 4);
	int y = panel->cards.back().bounds.bottom + VSScale(panel, kGutter) + VSScale(panel, 8);

	const wchar_t* sections[2] = { L"STRUTTURA", L"STILE" };
	int sectionIndex = 0;
	for (int index = 0; index < kParameterCount; ++index) {
		if (index == 0 || index == 8) {
			RECT label = { margin, y, client.right - margin, y + VSScale(panel, 18) };
			VSDrawText(device, sections[sectionIndex++], label, panel->fontEyebrow,
					   panel->theme.quiet, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
			y += VSScale(panel, 20);
		}
		RECT label = { margin, y, margin + VSScale(panel, 112), y + VSScale(panel, 20) };
		VSDrawText(device, kParameters[index].label, label, panel->fontBody,
				   panel->theme.muted, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
		y += VSScale(panel, 24);
	}

	RECT wave = { margin, y, client.right - margin, y + VSScale(panel, 18) };
	VSDrawText(device, L"ONDULAZIONE", wave, panel->fontEyebrow,
			   panel->theme.quiet, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
	y += VSScale(panel, 20) + VSScale(panel, 30);

	RECT generation = { margin, y, client.right - margin, y + VSScale(panel, 18) };
	VSDrawText(device, L"GENERAZIONE", generation, panel->fontEyebrow,
			   panel->theme.quiet, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
	y += VSScale(panel, 20) + VSScale(panel, 26) + VSScale(panel, 32) +
		 VSScale(panel, 32) + VSScale(panel, 38);

	RECT status = { margin, y, client.right - margin, y + VSScale(panel, 20) };
	VSDrawText(device, panel->status, status, panel->fontSmall,
			   panel->theme.quiet, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}

/// Disegno su bitmap di appoggio: senza doppio buffer lo scorrimento sfarfalla.
void VSPaintBuffered(VSPanel* panel, HWND window, bool header)
{
	PAINTSTRUCT paint;
	HDC device = BeginPaint(window, &paint);
	RECT client;
	GetClientRect(window, &client);

	HDC memory = CreateCompatibleDC(device);
	HBITMAP bitmap = CreateCompatibleBitmap(device, client.right, client.bottom);
	HBITMAP previous = static_cast<HBITMAP>(SelectObject(memory, bitmap));

	if (header) VSPaintHeader(panel, memory, client);
	else VSPaintCards(panel, memory, client);

	BitBlt(device, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
	SelectObject(memory, previous);
	DeleteObject(bitmap);
	DeleteDC(memory);
	EndPaint(window, &paint);
}

// ---------------------------------------------------------------------------
// Procedure di finestra
// ---------------------------------------------------------------------------

VSPanel* VSPanelFrom(HWND window)
{
	return reinterpret_cast<VSPanel*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

void VSHandleCommand(VSPanel* panel, WPARAM wParam)
{
	const int identifier = LOWORD(wParam);
	const int notification = HIWORD(wParam);

	if (identifier == kIDSearch && notification == EN_CHANGE) {
		VSRelayout(panel);
		return;
	}
	if (identifier == kIDCategory && notification == CBN_SELCHANGE) {
		VSRelayout(panel);
		return;
	}
	if (notification != BN_CLICKED) return;

	if (identifier == kIDGenerate) {
		VSGenerateNow(panel);
	}
	else if (identifier == kIDNewSeed) {
		VSSetSlider(panel, 0, static_cast<double>(rand() % 9999 + 1));
		if (!VSAutomaticOn(panel)) VSGenerateNow(panel);
	}
	else if (identifier == kIDReset) {
		for (int index = 0; index < kParameterCount; ++index) {
			VSSetSlider(panel, index, kParameters[index].defaultValue);
		}
		for (int index = 0; index < 4; ++index) {
			SendMessageW(panel->wave[index], BM_SETCHECK,
						 index == 1 ? BST_CHECKED : BST_UNCHECKED, 0);
		}
		VSWritePreference("wave", 1);
		panel->status = L"Valori predefiniti ripristinati";
		InvalidateRect(panel->canvas, nullptr, FALSE);
	}
	else if (identifier == kIDGroup) {
		const bool on = SendMessageW(panel->group, BM_GETCHECK, 0, 0) == BST_CHECKED;
		VSWritePreference("group", on ? 1 : 0);
		EnableWindow(panel->replace, on);
	}
	else if (identifier == kIDReplace) {
		VSWritePreference("replace",
			SendMessageW(panel->replace, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
	}
	else if (identifier == kIDAutomatic) {
		VSWritePreference("automatic", VSAutomaticOn(panel) ? 1 : 0);
	}
	else if (identifier == kIDAdvanced) {
		VSWritePreference("advanced", VSAdvancedOn(panel) ? 1 : 0);
		// I due parametri con limite esteso vanno riportati dentro il nuovo
		// intervallo, altrimenti il cursore mostrerebbe un valore che non c'è.
		for (int index = 0; index < kParameterCount; ++index) {
			if (kParameters[index].advancedMaximum > 0.0) {
				VSSetSlider(panel, index, VSValueFor(panel, index));
			}
		}
	}
	else if (identifier >= kIDProjPresetFirst && identifier < kIDProjPresetFirst + 3) {
		VSProjectionSettings settings = VSProjectionGet();
		if (identifier == kIDProjPresetFirst) {
			settings.leftAngle = 30.0;
			settings.rightAngle = 30.0;
		}
		else if (identifier == kIDProjPresetFirst + 1) {
			settings.leftAngle = 7.0;
			settings.rightAngle = 42.0;
		}
		else {
			return;   // «Libera» lascia gli angoli come sono
		}
		VSStoreProjection(settings);
		SendMessageW(panel->projLeft, TBM_SETPOS, TRUE,
					 static_cast<LPARAM>(std::lround(settings.leftAngle)));
		SendMessageW(panel->projRight, TBM_SETPOS, TRUE,
					 static_cast<LPARAM>(std::lround(settings.rightAngle)));
		InvalidateRect(panel->canvas, nullptr, FALSE);
	}
	else if (identifier >= kIDProjPlaneFirst && identifier < kIDProjPlaneFirst + 3) {
		VSProjectionSettings settings = VSProjectionGet();
		settings.plane = identifier - kIDProjPlaneFirst;
		VSStoreProjection(settings);
	}
	else if (identifier == kIDProjSnap) {
		VSProjectionSettings settings = VSProjectionGet();
		settings.snapLine =
			SendMessageW(panel->projSnap, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0;
		VSStoreProjection(settings);
	}
	else if (identifier >= kIDWaveFirst && identifier < kIDWaveFirst + 4) {
		VSWritePreference("wave", identifier - kIDWaveFirst);
		if (VSAutomaticOn(panel)) VSGenerateNow(panel);
	}
}

/// Il trackbar di Windows disegna un pomello a goccia colorato con l'accento di
/// sistema. Qui viene ridisegnato: traccia sottile e tacca verticale, come nel
/// pannello macOS.
LRESULT VSDrawSlider(VSPanel* panel, NMCUSTOMDRAW* draw)
{
	if (draw->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
	if (draw->dwDrawStage != CDDS_ITEMPREPAINT) return CDRF_DODEFAULT;

	Gdiplus::Graphics graphics(draw->hdc);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

	if (draw->dwItemSpec == TBCD_CHANNEL) {
		RECT bounds;
		GetClientRect(draw->hdr.hwndFrom, &bounds);
		const float height = 3.0f;
		const float centre = (bounds.bottom - bounds.top) * 0.5f;
		Gdiplus::RectF track(static_cast<Gdiplus::REAL>(bounds.left + 6),
							 centre - height * 0.5f,
							 static_cast<Gdiplus::REAL>(bounds.right - bounds.left - 12),
							 height);
		Gdiplus::SolidBrush rest(VSToGdi(panel->theme.line));
		Gdiplus::GraphicsPath* shape = VSRoundedPath(track, height * 0.5f);
		graphics.FillPath(&rest, shape);
		delete shape;

		const LRESULT minimum = SendMessageW(draw->hdr.hwndFrom, TBM_GETRANGEMIN, 0, 0);
		const LRESULT maximum = SendMessageW(draw->hdr.hwndFrom, TBM_GETRANGEMAX, 0, 0);
		const LRESULT position = SendMessageW(draw->hdr.hwndFrom, TBM_GETPOS, 0, 0);
		if (maximum > minimum) {
			const float ratio = static_cast<float>(position - minimum) /
								static_cast<float>(maximum - minimum);
			Gdiplus::RectF filled(track.X, track.Y, track.Width * ratio, track.Height);
			if (filled.Width > 0.5f) {
				Gdiplus::SolidBrush done(VSToGdi(panel->theme.ink));
				Gdiplus::GraphicsPath* progress = VSRoundedPath(filled, height * 0.5f);
				graphics.FillPath(&done, progress);
				delete progress;
			}
		}
		return CDRF_SKIPDEFAULT;
	}

	if (draw->dwItemSpec == TBCD_THUMB) {
		const float width = 3.0f;
		const float height = 14.0f;
		const float centreX = (draw->rc.left + draw->rc.right) * 0.5f;
		const float centreY = (draw->rc.top + draw->rc.bottom) * 0.5f;
		Gdiplus::RectF mark(centreX - width * 0.5f, centreY - height * 0.5f, width, height);

		Gdiplus::SolidBrush halo(VSToGdi(panel->theme.paper));
		Gdiplus::GraphicsPath* outline =
			VSRoundedPath(Gdiplus::RectF(mark.X - 1.5f, mark.Y - 1.5f,
										 mark.Width + 3.0f, mark.Height + 3.0f), 2.5f);
		graphics.FillPath(&halo, outline);
		delete outline;

		Gdiplus::SolidBrush ink(VSToGdi(panel->theme.ink));
		Gdiplus::GraphicsPath* bar = VSRoundedPath(mark, 1.5f);
		graphics.FillPath(&ink, bar);
		delete bar;
		return CDRF_SKIPDEFAULT;
	}

	return CDRF_DODEFAULT;
}

LRESULT CALLBACK VSCanvasProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	VSPanel* panel = VSPanelFrom(window);
	switch (message) {
	case WM_NOTIFY: {
		if (!panel) break;
		NMHDR* header = reinterpret_cast<NMHDR*>(lParam);
		if (header->code == NM_CUSTOMDRAW) {
			if (header->hwndFrom == panel->projLeft ||
				header->hwndFrom == panel->projRight) {
				return VSDrawSlider(panel, reinterpret_cast<NMCUSTOMDRAW*>(lParam));
			}
			for (int index = 0; index < kParameterCount; ++index) {
				if (panel->sliders[index] == header->hwndFrom) {
					return VSDrawSlider(panel, reinterpret_cast<NMCUSTOMDRAW*>(lParam));
				}
			}
		}
		break;
	}
	case WM_PAINT:
		if (panel) VSPaintBuffered(panel, window, false);
		return 0;
	case WM_ERASEBKGND:
		return 1;
	case WM_LBUTTONDOWN: {
		if (!panel) break;
		const POINT click = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		for (size_t index = 0; index < panel->cards.size(); ++index) {
			if (PtInRect(&panel->cards[index].bounds, click)) {
				panel->selectedModule = panel->cards[index].moduleID;
				if (panel->activateTool) {
					panel->activateTool(panel->context, panel->selectedModule);
				}
				VSRelayout(panel);
				break;
			}
		}
		return 0;
	}
	case WM_HSCROLL: {
		if (!panel) break;
		HWND slider = reinterpret_cast<HWND>(lParam);

		if (slider == panel->projLeft || slider == panel->projRight) {
			VSProjectionSettings settings = VSProjectionGet();
			const double value =
				static_cast<double>(SendMessageW(slider, TBM_GETPOS, 0, 0));
			if (slider == panel->projLeft) settings.leftAngle = value;
			else settings.rightAngle = value;
			VSStoreProjection(settings);

			// Muovere un cursore a mano significa uscire dai preset.
			const bool isometric = std::abs(settings.leftAngle - 30.0) < 0.05 &&
								   std::abs(settings.rightAngle - 30.0) < 0.05;
			const bool dimetric = std::abs(settings.leftAngle - 7.0) < 0.05 &&
								  std::abs(settings.rightAngle - 42.0) < 0.05;
			const int preset = isometric ? 0 : (dimetric ? 1 : 2);
			for (int index = 0; index < 3; ++index) {
				SendMessageW(panel->projPreset[index], BM_SETCHECK,
							 index == preset ? BST_CHECKED : BST_UNCHECKED, 0);
			}
			InvalidateRect(panel->canvas, nullptr, FALSE);
			return 0;
		}

		for (int index = 0; index < kParameterCount; ++index) {
			if (panel->sliders[index] != slider) continue;
			VSWritePreference(kParameters[index].key, VSValueFor(panel, index));
			VSSyncValueField(panel, index);
			if (LOWORD(wParam) == TB_ENDTRACK && VSAutomaticOn(panel)) {
				VSGenerateNow(panel);
			}
			break;
		}
		return 0;
	}
	case WM_COMMAND:
		if (panel) VSHandleCommand(panel, wParam);
		return 0;
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLORBTN: {
		if (!panel) break;
		HDC device = reinterpret_cast<HDC>(wParam);
		SetBkMode(device, TRANSPARENT);
		SetTextColor(device, panel->theme.ink);
		return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
	}
	case WM_CTLCOLOREDIT: {
		if (!panel) break;
		HDC device = reinterpret_cast<HDC>(wParam);
		SetTextColor(device, panel->theme.ink);
		SetBkColor(device, panel->theme.paper);
		return reinterpret_cast<LRESULT>(panel->fieldBrush);
	}
	default:
		break;
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK VSViewportProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	VSPanel* panel = VSPanelFrom(window);
	switch (message) {
	case WM_VSCROLL: {
		if (!panel) break;
		SCROLLINFO info = {};
		info.cbSize = sizeof(info);
		info.fMask = SIF_ALL;
		GetScrollInfo(window, SB_VERT, &info);
		const int previous = panel->scrollY;
		switch (LOWORD(wParam)) {
		case SB_LINEUP:   panel->scrollY -= 30; break;
		case SB_LINEDOWN: panel->scrollY += 30; break;
		case SB_PAGEUP:   panel->scrollY -= info.nPage; break;
		case SB_PAGEDOWN: panel->scrollY += info.nPage; break;
		case SB_THUMBTRACK:
		case SB_THUMBPOSITION: panel->scrollY = info.nTrackPos; break;
		default: break;
		}
		panel->scrollY = max(0, panel->scrollY);
		if (panel->scrollY != previous) VSUpdateScroll(panel);
		return 0;
	}
	case WM_MOUSEWHEEL: {
		if (!panel) break;
		const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		panel->scrollY = max(0, panel->scrollY - delta / 3);
		VSUpdateScroll(panel);
		return 0;
	}
	case WM_SIZE:
		if (panel) VSRelayout(panel);
		return 0;
	case WM_ERASEBKGND:
		return 1;
	default:
		break;
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK VSHostProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	VSPanel* panel = VSPanelFrom(window);
	switch (message) {
	case WM_PAINT:
		if (panel) VSPaintBuffered(panel, window, true);
		return 0;
	case WM_ERASEBKGND:
		return 1;
	case WM_COMMAND:
		if (panel) VSHandleCommand(panel, wParam);
		return 0;
	case WM_SETTINGCHANGE:
		if (panel) {
			panel->theme = VSCurrentTheme();
			if (panel->fieldBrush) DeleteObject(panel->fieldBrush);
			panel->fieldBrush = CreateSolidBrush(panel->theme.paper);
			InvalidateRect(panel->host, nullptr, TRUE);
			InvalidateRect(panel->canvas, nullptr, TRUE);
		}
		return 0;
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX: {
		if (!panel) break;
		HDC device = reinterpret_cast<HDC>(wParam);
		SetTextColor(device, panel->theme.ink);
		SetBkColor(device, panel->theme.paper);
		return reinterpret_cast<LRESULT>(panel->fieldBrush);
	}
	default:
		break;
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

void VSRegisterClasses()
{
	static bool registered = false;
	if (registered) return;
	registered = true;

	HMODULE module = VSModuleHandle();
	WNDCLASSEXW description = {};
	description.cbSize = sizeof(description);
	description.style = CS_HREDRAW | CS_VREDRAW;
	description.hInstance = module;
	description.hCursor = LoadCursorW(nullptr, IDC_ARROW);

	description.lpfnWndProc = VSHostProc;
	description.lpszClassName = kHostClass;
	RegisterClassExW(&description);

	description.lpfnWndProc = VSViewportProc;
	description.lpszClassName = kViewportClass;
	RegisterClassExW(&description);

	description.lpfnWndProc = VSCanvasProc;
	description.lpszClassName = kCanvasClass;
	RegisterClassExW(&description);
}

HFONT VSCreateFont(int points, int weight, int dpi)
{
	NONCLIENTMETRICSW metrics = {};
	metrics.cbSize = sizeof(metrics);
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
	LOGFONTW description = metrics.lfMessageFont;
	description.lfHeight = -MulDiv(points, dpi, 72);
	description.lfWeight = weight;
	return CreateFontIndirectW(&description);
}

HWND VSCreateChild(VSPanel* panel, HWND parent, const wchar_t* className,
				   const wchar_t* text, DWORD style, int identifier)
{
	HWND window = CreateWindowExW(
		0, className, text, WS_CHILD | style, 0, 0, 10, 10, parent,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)),
		VSModuleHandle(), nullptr);
	if (window) SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(panel->fontBody), TRUE);
	return window;
}

void VSBuildControls(VSPanel* panel)
{
	const int margin = VSScale(panel, kMargin);

	panel->search = CreateWindowExW(
		WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
		margin, VSScale(panel, 56), 100, VSScale(panel, 24), panel->host,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIDSearch)),
		VSModuleHandle(), nullptr);
	SendMessageW(panel->search, WM_SETFONT, reinterpret_cast<WPARAM>(panel->fontBody), TRUE);
	SendMessageW(panel->search, EM_SETCUEBANNER, TRUE,
				 reinterpret_cast<LPARAM>(L"Cerca un modulo"));

	panel->category = CreateWindowExW(
		0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
		margin, VSScale(panel, 86), 100, VSScale(panel, 200), panel->host,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIDCategory)),
		VSModuleHandle(), nullptr);
	SendMessageW(panel->category, WM_SETFONT, reinterpret_cast<WPARAM>(panel->fontBody), TRUE);
	const wchar_t* categories[] = { L"Tutti", L"Disegno", L"Geometria",
									L"Aspetto", L"Workflow", L"Sistema" };
	for (int index = 0; index < 6; ++index) {
		SendMessageW(panel->category, CB_ADDSTRING, 0,
					 reinterpret_cast<LPARAM>(categories[index]));
	}
	SendMessageW(panel->category, CB_SETCURSEL, 0, 0);

	for (int index = 0; index < kParameterCount; ++index) {
		panel->sliders[index] = VSCreateChild(
			panel, panel->canvas, TRACKBAR_CLASSW, L"",
			TBS_HORZ | TBS_NOTICKS, kIDSliderFirst + index);
		SendMessageW(panel->sliders[index], TBM_SETRANGE, TRUE,
					 MAKELPARAM(0, kSliderRange));
		panel->values[index] = CreateWindowExW(
			WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | ES_RIGHT | ES_AUTOHSCROLL, 0, 0, 10, 10, panel->canvas,
			reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIDValueFirst + index)),
			VSModuleHandle(), nullptr);
		SendMessageW(panel->values[index], WM_SETFONT,
					 reinterpret_cast<WPARAM>(panel->fontBody), TRUE);
		VSSetSlider(panel, index,
					VSReadPreference(kParameters[index].key,
									 kParameters[index].defaultValue));
	}

	const wchar_t* waveLabels[] = { L"Nessuna", L"Bassa", L"Media", L"Alta" };
	for (int index = 0; index < 4; ++index) {
		panel->wave[index] = VSCreateChild(
			panel, panel->canvas, L"BUTTON", waveLabels[index],
			BS_AUTORADIOBUTTON | (index == 0 ? WS_GROUP : 0), kIDWaveFirst + index);
	}
	const int storedWave = static_cast<int>(VSReadPreference("wave", 1));
	SendMessageW(panel->wave[max(0, min(3, storedWave))], BM_SETCHECK, BST_CHECKED, 0);

	panel->automatic = VSCreateChild(panel, panel->canvas, L"BUTTON", L"Auto",
									 BS_AUTOCHECKBOX, kIDAutomatic);
	panel->advanced = VSCreateChild(panel, panel->canvas, L"BUTTON", L"Avanzato",
									BS_AUTOCHECKBOX, kIDAdvanced);
	panel->group = VSCreateChild(panel, panel->canvas, L"BUTTON", L"Raggruppa",
								 BS_AUTOCHECKBOX, kIDGroup);
	panel->replace = VSCreateChild(panel, panel->canvas, L"BUTTON", L"Sostituisci",
								   BS_AUTOCHECKBOX, kIDReplace);
	SendMessageW(panel->group, BM_SETCHECK,
				 VSReadPreference("group", 1) != 0 ? BST_CHECKED : BST_UNCHECKED, 0);
	SendMessageW(panel->replace, BM_SETCHECK,
				 VSReadPreference("replace", 1) != 0 ? BST_CHECKED : BST_UNCHECKED, 0);
	SendMessageW(panel->automatic, BM_SETCHECK,
				 VSReadPreference("automatic", 0) != 0 ? BST_CHECKED : BST_UNCHECKED, 0);
	SendMessageW(panel->advanced, BM_SETCHECK,
				 VSReadPreference("advanced", 0) != 0 ? BST_CHECKED : BST_UNCHECKED, 0);

	panel->newSeed = VSCreateChild(panel, panel->canvas, L"BUTTON", L"Nuovo seme",
								   BS_PUSHBUTTON, kIDNewSeed);
	panel->reset = VSCreateChild(panel, panel->canvas, L"BUTTON", L"Ripristina",
								 BS_PUSHBUTTON, kIDReset);
	panel->generate = VSCreateChild(panel, panel->canvas, L"BUTTON", L"Genera albero",
									BS_DEFPUSHBUTTON, kIDGenerate);

	// --- Projection Studio ---
	VSRestoreProjection();
	const VSProjectionSettings projection = VSProjectionGet();

	const wchar_t* presets[] = { L"Isometrica", L"Dimetrica", L"Libera" };
	for (int index = 0; index < 3; ++index) {
		panel->projPreset[index] = VSCreateChild(
			panel, panel->canvas, L"BUTTON", presets[index],
			BS_AUTORADIOBUTTON | (index == 0 ? WS_GROUP : 0), kIDProjPresetFirst + index);
	}

	panel->projLeft = VSCreateChild(panel, panel->canvas, TRACKBAR_CLASSW, L"",
									TBS_HORZ | TBS_NOTICKS, kIDProjLeft);
	SendMessageW(panel->projLeft, TBM_SETRANGE, TRUE, MAKELPARAM(1, 89));
	SendMessageW(panel->projLeft, TBM_SETPOS, TRUE,
				 static_cast<LPARAM>(std::lround(projection.leftAngle)));

	panel->projRight = VSCreateChild(panel, panel->canvas, TRACKBAR_CLASSW, L"",
									 TBS_HORZ | TBS_NOTICKS, kIDProjRight);
	SendMessageW(panel->projRight, TBM_SETRANGE, TRUE, MAKELPARAM(1, 89));
	SendMessageW(panel->projRight, TBM_SETPOS, TRUE,
				 static_cast<LPARAM>(std::lround(projection.rightAngle)));

	const wchar_t* planes[] = { L"Superiore", L"Sinistro", L"Destro" };
	for (int index = 0; index < 3; ++index) {
		panel->projPlane[index] = VSCreateChild(
			panel, panel->canvas, L"BUTTON", planes[index],
			BS_AUTORADIOBUTTON | (index == 0 ? WS_GROUP : 0), kIDProjPlaneFirst + index);
	}
	SendMessageW(panel->projPlane[max(0, min(2, projection.plane))],
				 BM_SETCHECK, BST_CHECKED, 0);

	panel->projSnap = VSCreateChild(panel, panel->canvas, L"BUTTON",
									L"Aggancia la linea agli assi",
									BS_AUTOCHECKBOX, kIDProjSnap);
	SendMessageW(panel->projSnap, BM_SETCHECK,
				 projection.snapLine ? BST_CHECKED : BST_UNCHECKED, 0);
}

}  // namespace

// ---------------------------------------------------------------------------
// Ponte C
// ---------------------------------------------------------------------------

extern "C" void* VSCreatePanelController(
	void* parentWindow,
	VSPanelActivateToolProc activateTool,
	VSPanelGenerateFractalProc generateFractal,
	void* context)
{
	HWND parent = reinterpret_cast<HWND>(parentWindow);
	if (!parent || !IsWindow(parent)) return nullptr;

	INITCOMMONCONTROLSEX controls = {};
	controls.dwSize = sizeof(controls);
	controls.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&controls);

	VSPanel* panel = new VSPanel();
	panel->parent = parent;
	panel->activateTool = activateTool;
	panel->generateFractal = generateFractal;
	panel->context = context;
	panel->theme = VSCurrentTheme();
	panel->fieldBrush = CreateSolidBrush(panel->theme.paper);

	Gdiplus::GdiplusStartupInput startup;
	Gdiplus::GdiplusStartup(&panel->gdiplusToken, &startup, nullptr);

	HDC screen = GetDC(parent);
	panel->dpi = GetDeviceCaps(screen, LOGPIXELSX);
	ReleaseDC(parent, screen);

	panel->fontTitle = VSCreateFont(10, FW_SEMIBOLD, panel->dpi);
	panel->fontCard = VSCreateFont(9, FW_SEMIBOLD, panel->dpi);
	panel->fontBody = VSCreateFont(8, FW_NORMAL, panel->dpi);
	panel->fontSmall = VSCreateFont(8, FW_NORMAL, panel->dpi);
	panel->fontEyebrow = VSCreateFont(7, FW_SEMIBOLD, panel->dpi);
	panel->fontMono = VSCreateFont(8, FW_NORMAL, panel->dpi);

	panel->icons.resize(kVSModuleCount, nullptr);
	for (int moduleID = 0; moduleID < kVSModuleCount; ++moduleID) {
		panel->icons[moduleID] = VSLoadIcon(VSResourceForKey(kVSModules[moduleID].key));
	}

	VSRegisterClasses();

	RECT client;
	GetClientRect(parent, &client);

	panel->host = CreateWindowExW(
		0, kHostClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
		0, 0, client.right, client.bottom, parent, nullptr,
		VSModuleHandle(), nullptr);
	if (!panel->host) {
		delete panel;
		return nullptr;
	}
	SetWindowLongPtrW(panel->host, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));

	panel->viewport = CreateWindowExW(
		0, kViewportClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL,
		0, 0, 10, 10, panel->host, nullptr, VSModuleHandle(), nullptr);
	SetWindowLongPtrW(panel->viewport, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));

	panel->canvas = CreateWindowExW(
		0, kCanvasClass, L"", WS_CHILD | WS_VISIBLE,
		0, 0, 10, 10, panel->viewport, nullptr, VSModuleHandle(), nullptr);
	SetWindowLongPtrW(panel->canvas, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));

	VSBuildControls(panel);
	VSResizePanelController(panel);
	return panel;
}

extern "C" void VSDestroyPanelController(void* controller)
{
	VSPanel* panel = static_cast<VSPanel*>(controller);
	if (!panel) return;

	for (size_t index = 0; index < panel->icons.size(); ++index) {
		delete panel->icons[index];
	}
	if (panel->host) DestroyWindow(panel->host);

	HFONT fonts[] = { panel->fontTitle, panel->fontCard, panel->fontBody,
					  panel->fontSmall, panel->fontEyebrow, panel->fontMono };
	for (int index = 0; index < 6; ++index) {
		if (fonts[index]) DeleteObject(fonts[index]);
	}
	if (panel->fieldBrush) DeleteObject(panel->fieldBrush);
	if (panel->gdiplusToken) Gdiplus::GdiplusShutdown(panel->gdiplusToken);
	delete panel;
}

extern "C" void VSResizePanelController(void* controller)
{
	VSPanel* panel = static_cast<VSPanel*>(controller);
	if (!panel || !panel->host) return;

	RECT client;
	GetClientRect(panel->parent, &client);
	const int width = client.right - client.left;
	const int height = client.bottom - client.top;
	MoveWindow(panel->host, 0, 0, width, height, TRUE);

	const int margin = VSScale(panel, kMargin);
	const int header = VSScale(panel, kHeaderHeight);
	MoveWindow(panel->search, margin, VSScale(panel, 56),
			   width - margin * 2, VSScale(panel, 24), TRUE);
	MoveWindow(panel->category, margin, VSScale(panel, 86),
			   width - margin * 2, VSScale(panel, 200), TRUE);
	MoveWindow(panel->viewport, 0, header, width, max(0, height - header), TRUE);

	VSRelayout(panel);
}

extern "C" void VSSelectPanelModule(void* controller, int moduleID)
{
	VSPanel* panel = static_cast<VSPanel*>(controller);
	if (!panel || moduleID < 0 || moduleID >= kVSModuleCount) return;
	panel->selectedModule = moduleID;
	VSRelayout(panel);
}
