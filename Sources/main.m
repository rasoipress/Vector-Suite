#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#include <sys/xattr.h>
#include <libkern/OSByteOrder.h>

#pragma mark - Costanti

static NSString * const VSPluginDirectoryDefaultsKey = @"VSPluginDirectoryPath";
static NSString * const VSIllustratorDirectoryDefaultsKey = @"VSIllustratorApplicationPath";

/// I quattro byte iniziali che identificano i formati Mach-O supportati.
static const uint32_t VSFatMagic       = 0xcafebabe;
static const uint32_t VSFatMagic64     = 0xcafebabf;
static const uint32_t VSMachMagic64    = 0xfeedfacf;

static const uint32_t VSCpuTypeX8664   = 0x01000007;
static const uint32_t VSCpuTypeArm64   = 0x0100000c;

/// Quoting POSIX per percorsi passati al comando amministrativo.
static NSString *VSShellQuote(NSString *value) {
    NSString *escaped = [value stringByReplacingOccurrencesOfString:@"'"
                                                          withString:@"'\\''"];
    return [NSString stringWithFormat:@"'%@'", escaped];
}

/// Quoting del comando shell dentro una stringa AppleScript.
static NSString *VSAppleScriptQuote(NSString *value) {
    NSString *escaped = [value stringByReplacingOccurrencesOfString:@"\\"
                                                          withString:@"\\\\"];
    escaped = [escaped stringByReplacingOccurrencesOfString:@"\""
                                                 withString:@"\\\""];
    return escaped;
}

#pragma mark - Descrittore di modulo

/// Ogni slot della libreria corrisponde a un modulo del bundle nativo unico.
@interface VSModuleDescriptor : NSObject
@property (copy) NSString *slot;
@property (copy) NSString *displayName;
@property (copy) NSString *bundleName;
@property (copy) NSString *bundleIdentifier;
@end

@implementation VSModuleDescriptor
+ (instancetype)slot:(NSString *)slot
                name:(NSString *)name
              bundle:(NSString *)bundle
          identifier:(NSString *)identifier {
    VSModuleDescriptor *descriptor = [[VSModuleDescriptor alloc] init];
    descriptor.slot = slot;
    descriptor.displayName = name;
    descriptor.bundleName = bundle;
    descriptor.bundleIdentifier = identifier;
    return descriptor;
}
@end

#pragma mark - Delegate

@interface VSAppDelegate : NSObject <NSApplicationDelegate, WKNavigationDelegate, WKScriptMessageHandler>
@property (strong) NSWindow *window;
@property (strong) WKWebView *webView;
@property (strong) NSURL *pluginDirectory;
@property (copy)   NSString *pluginDirectorySource;
@property (strong) NSArray<VSModuleDescriptor *> *modules;
@property (assign) BOOL rendererReady;
@end

@implementation VSAppDelegate

#pragma mark Ciclo di vita

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    self.modules = [self moduleCatalogue];
    [self resolvePluginDirectory];
    [self buildMenuBar];
    [self buildWindow];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

#pragma mark Catalogo

- (NSArray<VSModuleDescriptor *> *)moduleCatalogue {
    return @[
        [VSModuleDescriptor slot:@"module-01" name:@"Precision Pen"    bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-02" name:@"Fluid Sketch"     bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-03" name:@"Ink Studio"       bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-04" name:@"Width Studio"     bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-05" name:@"Path Studio"      bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-06" name:@"Geometry Lab"     bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-07" name:@"Collision Align"  bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-08" name:@"Mirror Studio"    bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-09" name:@"Shape Reform"     bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-10" name:@"Live Style"       bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-11" name:@"Color Lab"        bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-12" name:@"Texture Lab"      bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-13" name:@"Stipple Lab"      bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-14" name:@"Randomize"        bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-15" name:@"Smart Find"       bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-16" name:@"Vector Repair"    bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-17" name:@"Raster Lab"       bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-18" name:@"Auto Save"        bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-19" name:@"Direct Settings"  bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-20" name:@"Suite Core"       bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-21" name:@"Projection Studio" bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"],
        [VSModuleDescriptor slot:@"module-22" name:@"Fractal Grove"    bundle:@"VectorSuiteNative" identifier:@"studio.vectorsuite.plugin.core"]
    ];
}

#pragma mark Risoluzione delle cartelle

- (NSArray<NSURL *> *)illustratorApplicationURLs {
    NSFileManager *fileManager = NSFileManager.defaultManager;
    NSMutableArray<NSURL *> *found = [NSMutableArray array];

    NSString *override = [NSUserDefaults.standardUserDefaults stringForKey:VSIllustratorDirectoryDefaultsKey];
    if (override.length > 0) {
        [found addObject:[NSURL fileURLWithPath:override isDirectory:YES]];
    }

    NSURL *applications = [NSURL fileURLWithPath:@"/Applications" isDirectory:YES];
    NSArray<NSURL *> *entries = [fileManager contentsOfDirectoryAtURL:applications
                                          includingPropertiesForKeys:nil
                                                             options:NSDirectoryEnumerationSkipsHiddenFiles
                                                               error:nil] ?: @[];
    NSArray<NSURL *> *sorted = [entries sortedArrayUsingComparator:^NSComparisonResult(NSURL *left, NSURL *right) {
        // Ordine decrescente: la versione più recente viene proposta per prima.
        return [right.lastPathComponent localizedStandardCompare:left.lastPathComponent];
    }];
    for (NSURL *entry in sorted) {
        if ([entry.lastPathComponent hasPrefix:@"Adobe Illustrator"]) {
            [found addObject:entry];
        }
    }
    return found;
}

/// Percorsi candidati in ordine di priorità. Il primo che contiene bundle `.aip`
/// vince; se nessuno ne contiene, si ripiega sul primo che esiste.
- (NSArray<NSDictionary *> *)candidatePluginDirectories {
    NSMutableArray<NSDictionary *> *candidates = [NSMutableArray array];
    NSString *override = [NSUserDefaults.standardUserDefaults stringForKey:VSPluginDirectoryDefaultsKey];
    if (override.length > 0) {
        [candidates addObject:@{ @"path": override, @"source": @"Cartella scelta manualmente" }];
    }

    for (NSURL *application in [self illustratorApplicationURLs]) {
        [candidates addObject:@{
            @"path": [application.path stringByAppendingPathComponent:@"Plug-ins.localized"],
            @"source": [NSString stringWithFormat:@"Installazione %@", application.lastPathComponent]
        }];
        [candidates addObject:@{
            @"path": [application.path stringByAppendingPathComponent:@"Plug-ins"],
            @"source": [NSString stringWithFormat:@"Installazione %@", application.lastPathComponent]
        }];
    }

    return candidates;
}

- (NSUInteger)bundleCountAtPath:(NSString *)path {
    NSURL *url = [NSURL fileURLWithPath:path isDirectory:YES];
    return [self bundleURLsInDirectory:url].count;
}

- (void)resolvePluginDirectory {
    NSFileManager *fileManager = NSFileManager.defaultManager;
    NSArray<NSDictionary *> *candidates = [self candidatePluginDirectories];

    NSDictionary *firstExisting = nil;
    for (NSDictionary *candidate in candidates) {
        NSString *path = candidate[@"path"];
        BOOL isDirectory = NO;
        if (![fileManager fileExistsAtPath:path isDirectory:&isDirectory] || !isDirectory) continue;
        if (!firstExisting) firstExisting = candidate;
        if ([self bundleCountAtPath:path] > 0) {
            self.pluginDirectory = [NSURL fileURLWithPath:path isDirectory:YES];
            self.pluginDirectorySource = candidate[@"source"];
            return;
        }
    }

    if (firstExisting) {
        self.pluginDirectory = [NSURL fileURLWithPath:firstExisting[@"path"] isDirectory:YES];
        self.pluginDirectorySource = [firstExisting[@"source"] stringByAppendingString:@" · nessun bundle"];
        return;
    }

    NSDictionary *fallback = candidates.firstObject;
    if (!fallback) {
        self.pluginDirectory = nil;
        self.pluginDirectorySource = @"Illustrator non trovato";
        return;
    }
    self.pluginDirectory = [NSURL fileURLWithPath:fallback[@"path"] isDirectory:YES];
    self.pluginDirectorySource = @"Cartella non trovata";
}

#pragma mark Lettura dei bundle

- (NSArray<NSURL *> *)bundleURLsInDirectory:(NSURL *)directory {
    if (!directory) return @[];
    NSArray<NSURL *> *entries = [NSFileManager.defaultManager contentsOfDirectoryAtURL:directory
                                                            includingPropertiesForKeys:nil
                                                                               options:NSDirectoryEnumerationSkipsHiddenFiles
                                                                                 error:nil] ?: @[];
    NSPredicate *predicate = [NSPredicate predicateWithBlock:^BOOL(NSURL *url, NSDictionary *bindings) {
        return [url.pathExtension.lowercaseString isEqualToString:@"aip"];
    }];
    return [entries filteredArrayUsingPredicate:predicate];
}

- (NSURL *)bundleURLForDescriptor:(VSModuleDescriptor *)descriptor {
    if (!self.pluginDirectory) return nil;
    NSFileManager *fileManager = NSFileManager.defaultManager;

    // 1. Corrispondenza esatta sul nome del bundle.
    NSURL *exact = [self.pluginDirectory URLByAppendingPathComponent:
                    [descriptor.bundleName stringByAppendingPathExtension:@"aip"]];
    if ([fileManager fileExistsAtPath:exact.path]) return exact;

    // 2. Corrispondenza sul bundle identifier: regge rinomine e varianti di case.
    for (NSURL *candidate in [self bundleURLsInDirectory:self.pluginDirectory]) {
        NSDictionary *info = [self infoDictionaryForBundle:candidate];
        NSString *identifier = info[@"CFBundleIdentifier"];
        if ([identifier isKindOfClass:NSString.class] &&
            [identifier caseInsensitiveCompare:descriptor.bundleIdentifier] == NSOrderedSame) {
            return candidate;
        }
    }
    return nil;
}

- (NSDictionary *)infoDictionaryForBundle:(NSURL *)bundleURL {
    NSURL *plistURL = [bundleURL URLByAppendingPathComponent:@"Contents/Info.plist"];
    NSDictionary *info = [NSDictionary dictionaryWithContentsOfURL:plistURL];
    return [info isKindOfClass:NSDictionary.class] ? info : @{};
}

/// Legge le architetture direttamente dall'header Mach-O: non dipende da
/// NSBundle, che non garantisce il supporto per l'estensione `.aip`.
- (NSArray<NSString *> *)architecturesForExecutable:(NSURL *)executableURL {
    NSData *data = [NSData dataWithContentsOfURL:executableURL
                                         options:NSDataReadingMappedIfSafe
                                           error:nil];
    if (data.length < 8) return @[];

    const uint8_t *bytes = data.bytes;
    NSMutableArray<NSString *> *architectures = [NSMutableArray array];

    uint32_t magic = OSReadBigInt32(bytes, 0);
    if (magic == VSFatMagic || magic == VSFatMagic64) {
        BOOL is64 = (magic == VSFatMagic64);
        uint32_t count = OSReadBigInt32(bytes, 4);
        if (count > 32) return @[];
        size_t stride = is64 ? 32 : 20;
        for (uint32_t index = 0; index < count; index += 1) {
            size_t offset = 8 + (size_t)index * stride;
            if (offset + 4 > data.length) break;
            uint32_t cpuType = OSReadBigInt32(bytes, offset);
            if (cpuType == VSCpuTypeArm64) [architectures addObject:@"arm64"];
            else if (cpuType == VSCpuTypeX8664) [architectures addObject:@"x86_64"];
        }
        return architectures;
    }

    // Binario non fat: su macOS l'header è sempre little-endian.
    if (OSReadLittleInt32(bytes, 0) == VSMachMagic64) {
        uint32_t cpuType = OSReadLittleInt32(bytes, 4);
        if (cpuType == VSCpuTypeArm64) [architectures addObject:@"arm64"];
        else if (cpuType == VSCpuTypeX8664) [architectures addObject:@"x86_64"];
    }
    return architectures;
}

- (BOOL)hasQuarantineFlagAtPath:(NSString *)path {
    ssize_t size = getxattr(path.fileSystemRepresentation, "com.apple.quarantine", NULL, 0, 0, XATTR_NOFOLLOW);
    return size >= 0;
}

/// Estrae la versione host da una stringa informativa che termina con "(30.0)".
- (NSString *)hostVersionFromInfoString:(id)value {
    if (![value isKindOfClass:NSString.class]) return @"";
    NSString *infoString = (NSString *)value;
    if (infoString.length == 0) return @"";
    NSRange open = [infoString rangeOfString:@"(" options:NSBackwardsSearch];
    NSRange close = [infoString rangeOfString:@")" options:NSBackwardsSearch];
    if (open.location == NSNotFound || close.location == NSNotFound || close.location <= open.location) return @"";
    NSRange inner = NSMakeRange(open.location + 1, close.location - open.location - 1);
    return [infoString substringWithRange:inner];
}

- (NSDictionary *)recordForDescriptor:(VSModuleDescriptor *)descriptor {
    NSMutableDictionary *record = [NSMutableDictionary dictionary];
    record[@"slot"] = descriptor.slot;
    record[@"expectedBundle"] = descriptor.bundleName;
    record[@"expectedIdentifier"] = descriptor.bundleIdentifier;

    NSURL *bundleURL = [self bundleURLForDescriptor:descriptor];
    if (!bundleURL) {
        record[@"detected"] = @NO;
        record[@"issues"] = @[@"Bundle non presente nella cartella plug-in."];
        return record;
    }

    NSFileManager *fileManager = NSFileManager.defaultManager;
    NSDictionary *info = [self infoDictionaryForBundle:bundleURL];
    NSString *executableName = info[@"CFBundleExecutable"] ?: descriptor.bundleName;
    NSURL *executableURL = [bundleURL URLByAppendingPathComponent:
                            [@"Contents/MacOS" stringByAppendingPathComponent:executableName]];

    NSArray<NSString *> *architectures = [self architecturesForExecutable:executableURL];
    BOOL hasExecutable = [fileManager fileExistsAtPath:executableURL.path];
    BOOL hasSignature = [fileManager fileExistsAtPath:
                         [bundleURL URLByAppendingPathComponent:@"Contents/_CodeSignature/CodeResources"].path];
    BOOL quarantined = [self hasQuarantineFlagAtPath:bundleURL.path];
    NSString *identifier = info[@"CFBundleIdentifier"] ?: @"";

    NSMutableArray<NSString *> *issues = [NSMutableArray array];
    if (!hasExecutable) {
        [issues addObject:@"Eseguibile mancante in Contents/MacOS."];
    } else if (![architectures containsObject:@"arm64"]) {
        [issues addObject:@"Nessuna slice arm64: non si carica in Illustrator nativo su Apple Silicon."];
    }
    if (!hasSignature) [issues addObject:@"Firma del codice assente."];
    if (quarantined)   [issues addObject:@"Attributo di quarantena presente: macOS può bloccare il caricamento."];
    if (identifier.length > 0 &&
        [identifier caseInsensitiveCompare:descriptor.bundleIdentifier] != NSOrderedSame) {
        [issues addObject:[NSString stringWithFormat:@"Identifier inatteso: %@", identifier]];
    }

    record[@"detected"] = @(hasExecutable);
    record[@"bundleName"] = info[@"CFBundleName"] ?: bundleURL.lastPathComponent.stringByDeletingPathExtension;
    record[@"identifier"] = identifier;
    record[@"version"] = info[@"CFBundleVersion"] ?: @"";
    record[@"hostVersion"] = [self hostVersionFromInfoString:info[@"CFBundleGetInfoString"]];
    record[@"architectures"] = architectures;
    record[@"signed"] = @(hasSignature);
    record[@"quarantined"] = @(quarantined);
    record[@"path"] = bundleURL.path;
    record[@"issues"] = issues;
    return record;
}

- (NSDictionary *)scanPayload {
    NSMutableArray<NSDictionary *> *records = [NSMutableArray arrayWithCapacity:self.modules.count];
    for (VSModuleDescriptor *descriptor in self.modules) {
        [records addObject:[self recordForDescriptor:descriptor]];
    }

    NSArray<NSURL *> *allBundles = [self bundleURLsInDirectory:self.pluginDirectory];
    NSMutableSet<NSString *> *known = [NSMutableSet set];
    for (VSModuleDescriptor *descriptor in self.modules) {
        [known addObject:descriptor.bundleName.lowercaseString];
    }
    NSMutableArray<NSString *> *extras = [NSMutableArray array];
    for (NSURL *bundle in allBundles) {
        NSString *name = bundle.lastPathComponent.stringByDeletingPathExtension;
        if (![known containsObject:name.lowercaseString]) [extras addObject:name];
    }

    return @{
        @"directory": self.pluginDirectory.path ?: @"",
        @"directorySource": self.pluginDirectorySource ?: @"",
        @"bundleCount": @(allBundles.count),
        @"extras": extras,
        @"modules": records
    };
}

#pragma mark Ponte con la webview

- (void)sendScanToRenderer {
    if (!self.rendererReady) return;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:[self scanPayload] options:0 error:nil];
    NSString *json = jsonData ? [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding] : nil;
    NSString *script = [NSString stringWithFormat:@"window.receiveScan(%@);", json ?: @"null"];
    [self.webView evaluateJavaScript:script completionHandler:nil];
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    self.rendererReady = YES;
    [self sendScanToRenderer];
}

- (void)userContentController:(WKUserContentController *)userContentController
      didReceiveScriptMessage:(WKScriptMessage *)message {
    if (![message.body isKindOfClass:NSDictionary.class]) return;
    NSDictionary *payload = (NSDictionary *)message.body;
    NSString *action = payload[@"action"];
    if (![action isKindOfClass:NSString.class]) return;

    if ([action isEqualToString:@"ready"] || [action isEqualToString:@"rescan"]) {
        self.rendererReady = YES;
        [self resolvePluginDirectory];
        [self sendScanToRenderer];
    } else if ([action isEqualToString:@"openFolder"]) {
        [self openPluginDirectory:nil];
    } else if ([action isEqualToString:@"chooseFolder"]) {
        [self choosePluginDirectory:nil];
    } else if ([action isEqualToString:@"launchIllustrator"]) {
        [self launchIllustrator:nil];
    } else if ([action isEqualToString:@"installIntoIllustrator"]) {
        [self installIntoIllustrator:nil];
    } else if ([action isEqualToString:@"reveal"]) {
        [self revealBundleForSlot:payload[@"slot"]];
    }
}

- (void)revealBundleForSlot:(id)slot {
    if (![slot isKindOfClass:NSString.class]) return;
    for (VSModuleDescriptor *descriptor in self.modules) {
        if (![descriptor.slot isEqualToString:slot]) continue;
        NSURL *bundleURL = [self bundleURLForDescriptor:descriptor];
        if (bundleURL) {
            [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:@[bundleURL]];
        }
        return;
    }
}

#pragma mark Azioni

- (void)launchIllustrator:(id)sender {
    NSArray<NSURL *> *applications = [self illustratorApplicationURLs];
    NSURL *executable = nil;
    for (NSURL *application in applications) {
        NSURL *candidate = [application URLByAppendingPathComponent:@"Adobe Illustrator.app"];
        if ([NSFileManager.defaultManager fileExistsAtPath:candidate.path]) {
            executable = candidate;
            break;
        }
        if ([application.pathExtension isEqualToString:@"app"]) {
            executable = application;
            break;
        }
    }

    if (!executable) {
        [self presentAlertWithTitle:@"Illustrator non trovato"
                            message:@"Nessuna installazione di Adobe Illustrator in /Applications."
                              style:NSAlertStyleWarning];
        return;
    }

    NSWorkspaceOpenConfiguration *configuration = [NSWorkspaceOpenConfiguration configuration];
    configuration.activates = YES;
    [NSWorkspace.sharedWorkspace openApplicationAtURL:executable
                                        configuration:configuration
                                    completionHandler:nil];
}

- (void)openPluginDirectory:(id)sender {
    if (![NSFileManager.defaultManager fileExistsAtPath:self.pluginDirectory.path]) {
        [self presentAlertWithTitle:@"Cartella non disponibile"
                            message:self.pluginDirectory.path ?: @""
                              style:NSAlertStyleWarning];
        return;
    }
    [NSWorkspace.sharedWorkspace openURL:self.pluginDirectory];
}

- (void)choosePluginDirectory:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseDirectories = YES;
    panel.canChooseFiles = NO;
    panel.allowsMultipleSelection = NO;
    panel.message = @"Seleziona la cartella Plug-ins che Illustrator carica all'avvio.";
    panel.prompt = @"Usa questa cartella";
    if (self.pluginDirectory) panel.directoryURL = self.pluginDirectory;

    if ([panel runModal] != NSModalResponseOK || !panel.URL) return;
    [NSUserDefaults.standardUserDefaults setObject:panel.URL.path forKey:VSPluginDirectoryDefaultsKey];
    [self resolvePluginDirectory];
    [self sendScanToRenderer];
}

- (void)resetPluginDirectory:(id)sender {
    [NSUserDefaults.standardUserDefaults removeObjectForKey:VSPluginDirectoryDefaultsKey];
    [self resolvePluginDirectory];
    [self sendScanToRenderer];
}

/// Restituisce il bundle nativo incorporato nell'app.
- (NSURL *)embeddedNativePluginURL {
    NSURL *url = [NSBundle.mainBundle.resourceURL
                  URLByAppendingPathComponent:@"Native/VectorSuiteNative.aip"
                  isDirectory:YES];
    return [NSFileManager.defaultManager fileExistsAtPath:url.path] ? url : nil;
}

/// Usa il pannello di autenticazione standard di macOS quando la cartella
/// Plug-ins appartiene a root. La destinazione viene sostituita soltanto dopo
/// aver copiato e verificato il nuovo bundle in un percorso temporaneo.
- (BOOL)installPluginAsAdministratorFrom:(NSURL *)source
                                      to:(NSURL *)destination
                               temporary:(NSURL *)temporary
                                   error:(NSError **)error {
    NSString *sourcePath = VSShellQuote(source.path);
    NSString *destinationPath = VSShellQuote(destination.path);
    NSString *temporaryPath = VSShellQuote(temporary.path);
    NSString *command = [NSString stringWithFormat:
        @"/bin/rm -rf %@ && "
         "/usr/bin/ditto --norsrc --noextattr --noacl %@ %@ && "
         "/usr/bin/codesign --verify --deep --strict %@ && "
         "/bin/rm -rf %@ && "
         "/bin/mv %@ %@ && "
         "/usr/bin/xattr -cr %@",
         temporaryPath,
         sourcePath, temporaryPath,
         temporaryPath,
         destinationPath,
         temporaryPath, destinationPath,
         destinationPath];
    NSString *appleSource = [NSString stringWithFormat:
        @"do shell script \"%@\" with administrator privileges",
        VSAppleScriptQuote(command)];
    NSAppleScript *script = [[NSAppleScript alloc] initWithSource:appleSource];
    NSDictionary *scriptError = nil;
    NSAppleEventDescriptor *result = [script executeAndReturnError:&scriptError];
    if (result) return YES;

    NSString *message = scriptError[NSAppleScriptErrorMessage] ?: @"Autenticazione amministrativa non riuscita.";
    if (error) {
        *error = [NSError errorWithDomain:@"studio.vectorsuite.install"
                                    code:[scriptError[NSAppleScriptErrorNumber] integerValue]
                                userInfo:@{NSLocalizedDescriptionKey: message}];
    }
    return NO;
}

/// Installa il solo bundle nativo Vector Suite nella cartella caricata
/// dall'installazione più recente di Illustrator.
- (void)installIntoIllustrator:(id)sender {
    NSFileManager *fileManager = NSFileManager.defaultManager;
    NSURL *target = nil;
    for (NSURL *application in [self illustratorApplicationURLs]) {
        NSURL *localized = [application URLByAppendingPathComponent:@"Plug-ins.localized"];
        NSURL *plain = [application URLByAppendingPathComponent:@"Plug-ins"];
        if ([fileManager fileExistsAtPath:localized.path]) { target = localized; break; }
        if ([fileManager fileExistsAtPath:plain.path]) { target = plain; break; }
    }

    if (!target) {
        [self presentAlertWithTitle:@"Cartella di destinazione non trovata"
                            message:@"Nessuna cartella Plug-ins in un'installazione di Adobe Illustrator."
                              style:NSAlertStyleWarning];
        return;
    }

    NSURL *source = [self embeddedNativePluginURL];
    if (!source) {
        [self presentAlertWithTitle:@"Bundle nativo non disponibile"
                            message:@"Ricompila l'app con scripts/build-macos.sh: la build deve incorporare VectorSuiteNative.aip."
                              style:NSAlertStyleWarning];
        return;
    }

    NSAlert *confirm = [[NSAlert alloc] init];
    confirm.messageText = @"Installare Vector Suite in Illustrator?";
    confirm.informativeText = [NSString stringWithFormat:@"Verrà installato il bundle nativo verificato:\n%@\n\nDestinazione:\n%@\n\nLa versione precedente con lo stesso nome verrà sostituita.",
                               source.path, target.path];
    [confirm addButtonWithTitle:@"Installa"];
    [confirm addButtonWithTitle:@"Annulla"];
    if ([confirm runModal] != NSAlertFirstButtonReturn) return;

    NSURL *destination = [target URLByAppendingPathComponent:source.lastPathComponent];
    NSURL *temporary = [target URLByAppendingPathComponent:@".VectorSuiteNative.installing"];
    NSError *error = nil;

    if (![fileManager isWritableFileAtPath:target.path]) {
        if (![self installPluginAsAdministratorFrom:source
                                                 to:destination
                                          temporary:temporary
                                              error:&error]) {
            [self presentAlertWithTitle:@"Installazione non riuscita"
                                message:error.localizedDescription ?: @"Errore sconosciuto"
                                  style:NSAlertStyleWarning];
            return;
        }
        [self presentAlertWithTitle:@"Vector Suite installato"
                            message:@"Riavvia Illustrator, quindi apri Finestra > Vector Suite. Tutti gli strumenti sono disponibili anche nell’editor della barra strumenti."
                              style:NSAlertStyleInformational];
        [self resolvePluginDirectory];
        [self sendScanToRenderer];
        return;
    }

    [fileManager removeItemAtURL:temporary error:nil];
    if (![fileManager copyItemAtURL:source toURL:temporary error:&error]) {
        [self presentAlertWithTitle:@"Installazione non riuscita"
                            message:error.localizedDescription ?: @"Errore sconosciuto"
                              style:NSAlertStyleWarning];
        return;
    }
    BOOL installed = NO;
    if ([fileManager fileExistsAtPath:destination.path]) {
        installed = [fileManager replaceItemAtURL:destination
                                    withItemAtURL:temporary
                                   backupItemName:nil
                                          options:0
                                 resultingItemURL:nil
                                            error:&error];
    } else {
        installed = [fileManager moveItemAtURL:temporary toURL:destination error:&error];
    }
    if (!installed) {
        [fileManager removeItemAtURL:temporary error:nil];
        [self presentAlertWithTitle:@"Installazione non riuscita"
                            message:error.localizedDescription ?: @"Errore sconosciuto"
                              style:NSAlertStyleWarning];
        return;
    }

    removexattr(destination.path.fileSystemRepresentation, "com.apple.quarantine", XATTR_NOFOLLOW);
    [self presentAlertWithTitle:@"Vector Suite installato"
                        message:@"Riavvia Illustrator, quindi apri Finestra > Vector Suite. Tutti gli strumenti sono disponibili anche nell’editor della barra strumenti."
                          style:NSAlertStyleInformational];
    [self resolvePluginDirectory];
    [self sendScanToRenderer];
}

- (void)rescan:(id)sender {
    [self resolvePluginDirectory];
    [self sendScanToRenderer];
}

- (void)selectModuleFromMenu:(NSMenuItem *)sender {
    NSString *identifier = sender.representedObject;
    if (![identifier isKindOfClass:NSString.class]) return;
    NSData *data = [NSJSONSerialization dataWithJSONObject:identifier
                                                  options:NSJSONWritingFragmentsAllowed
                                                    error:nil];
    NSString *json = data ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : nil;
    NSString *script = [NSString stringWithFormat:@"window.selectModule(%@);", json ?: @"null"];
    [self.webView evaluateJavaScript:script completionHandler:nil];
    [self.window makeKeyAndOrderFront:nil];
}

- (void)presentAlertWithTitle:(NSString *)title message:(NSString *)message style:(NSAlertStyle)style {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = title;
    alert.informativeText = message ?: @"";
    alert.alertStyle = style;
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

#pragma mark Interfaccia

- (void)buildWindow {
    NSRect frame = NSMakeRect(0, 0, 1260, 820);
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:(NSWindowStyleMaskTitled |
                                                         NSWindowStyleMaskClosable |
                                                         NSWindowStyleMaskMiniaturizable |
                                                         NSWindowStyleMaskResizable |
                                                         NSWindowStyleMaskFullSizeContentView)
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"Vector Suite";
    self.window.titlebarAppearsTransparent = YES;
    self.window.titleVisibility = NSWindowTitleHidden;
    self.window.minSize = NSMakeSize(960, 640);
    self.window.backgroundColor = NSColor.windowBackgroundColor;

    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    [configuration.userContentController addScriptMessageHandler:self name:@"native"];
    configuration.preferences.javaScriptCanOpenWindowsAutomatically = NO;

    self.webView = [[WKWebView alloc] initWithFrame:frame configuration:configuration];
    self.webView.navigationDelegate = self;
    self.webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.window.contentView = self.webView;

    NSURL *resourceRoot = NSBundle.mainBundle.resourceURL;
    NSURL *indexURL = [resourceRoot URLByAppendingPathComponent:@"ui/index.html"];
    [self.webView loadFileURL:indexURL allowingReadAccessToURL:resourceRoot];

    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)buildMenuBar {
    NSMenu *menuBar = [[NSMenu alloc] init];

    NSMenuItem *applicationItem = [[NSMenuItem alloc] init];
    [menuBar addItem:applicationItem];
    NSMenu *applicationMenu = [[NSMenu alloc] initWithTitle:@"Vector Suite"];
    [applicationMenu addItemWithTitle:@"Informazioni su Vector Suite"
                               action:@selector(orderFrontStandardAboutPanel:)
                        keyEquivalent:@""];
    [applicationMenu addItem:[NSMenuItem separatorItem]];
    [applicationMenu addItemWithTitle:@"Nascondi Vector Suite" action:@selector(hide:) keyEquivalent:@"h"];
    [applicationMenu addItem:[NSMenuItem separatorItem]];
    [applicationMenu addItemWithTitle:@"Esci da Vector Suite"
                               action:@selector(terminate:)
                        keyEquivalent:@"q"];
    applicationItem.submenu = applicationMenu;

    NSMenuItem *editItem = [[NSMenuItem alloc] init];
    [menuBar addItem:editItem];
    NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Modifica"];
    [editMenu addItemWithTitle:@"Taglia" action:@selector(cut:) keyEquivalent:@"x"];
    [editMenu addItemWithTitle:@"Copia" action:@selector(copy:) keyEquivalent:@"c"];
    [editMenu addItemWithTitle:@"Incolla" action:@selector(paste:) keyEquivalent:@"v"];
    [editMenu addItemWithTitle:@"Seleziona tutto" action:@selector(selectAll:) keyEquivalent:@"a"];
    editItem.submenu = editMenu;

    NSMenuItem *suiteItem = [[NSMenuItem alloc] init];
    [menuBar addItem:suiteItem];
    NSMenu *suiteMenu = [[NSMenu alloc] initWithTitle:@"Suite"];
    [suiteMenu addItem:[self menuItemWithTitle:@"Apri Illustrator"
                                        action:@selector(launchIllustrator:)
                                           key:@"i"
                                     modifiers:NSEventModifierFlagCommand | NSEventModifierFlagShift]];
    [suiteMenu addItem:[self menuItemWithTitle:@"Rileva nuovamente"
                                        action:@selector(rescan:)
                                           key:@"r"
                                     modifiers:NSEventModifierFlagCommand]];
    [suiteMenu addItem:[NSMenuItem separatorItem]];
    [suiteMenu addItem:[self menuItemWithTitle:@"Apri cartella plug-in"
                                        action:@selector(openPluginDirectory:)
                                           key:@"o"
                                     modifiers:NSEventModifierFlagCommand]];
    [suiteMenu addItem:[self menuItemWithTitle:@"Scegli cartella plug-in…"
                                        action:@selector(choosePluginDirectory:)
                                           key:@"o"
                                     modifiers:NSEventModifierFlagCommand | NSEventModifierFlagShift]];
    [suiteMenu addItem:[self menuItemWithTitle:@"Ripristina cartella automatica"
                                        action:@selector(resetPluginDirectory:)
                                           key:@""
                                     modifiers:0]];
    [suiteMenu addItem:[NSMenuItem separatorItem]];
    [suiteMenu addItem:[self menuItemWithTitle:@"Installa Vector Suite in Illustrator…"
                                        action:@selector(installIntoIllustrator:)
                                           key:@""
                                     modifiers:0]];
    suiteItem.submenu = suiteMenu;

    NSMenuItem *modulesItem = [[NSMenuItem alloc] init];
    [menuBar addItem:modulesItem];
    NSMenu *modulesMenu = [[NSMenu alloc] initWithTitle:@"Moduli"];
    for (VSModuleDescriptor *descriptor in self.modules) {
        NSMenuItem *item = [self menuItemWithTitle:descriptor.displayName
                                           action:@selector(selectModuleFromMenu:)
                                              key:@""
                                        modifiers:0];
        item.representedObject = descriptor.slot;
        [modulesMenu addItem:item];
    }
    modulesItem.submenu = modulesMenu;

    NSMenuItem *windowItem = [[NSMenuItem alloc] init];
    [menuBar addItem:windowItem];
    NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:@"Finestra"];
    [windowMenu addItemWithTitle:@"Minimizza" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    windowItem.submenu = windowMenu;
    NSApp.windowsMenu = windowMenu;

    NSApp.mainMenu = menuBar;
}

- (NSMenuItem *)menuItemWithTitle:(NSString *)title
                           action:(SEL)action
                              key:(NSString *)key
                        modifiers:(NSEventModifierFlags)modifiers {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key];
    item.target = self;
    item.keyEquivalentModifierMask = modifiers;
    return item;
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *application = NSApplication.sharedApplication;
        [application setActivationPolicy:NSApplicationActivationPolicyRegular];
        VSAppDelegate *delegate = [[VSAppDelegate alloc] init];
        application.delegate = delegate;
        [application run];
    }
    return 0;
}
