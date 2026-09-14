#ifndef VECTOR_SUITE_PROJECTION_H
#define VECTOR_SUITE_PROJECTION_H

// Impostazioni condivise dai quattro strumenti di Projection Studio.
//
// Prima gli assi erano fissi a 30° e non c'era modo di toccarli: si poteva
// disegnare solo in isometrica, e solo sul piano superiore. Sono i parametri
// elementari del disegno assonometrico, e mancavano tutti.
//
// La geometria è quella classica del disegno tecnico, non l'imitazione di un
// prodotto altrui: due assi orizzontali inclinati di un angolo scelto e un
// terzo asse verticale. Cambiando i due angoli si passa da isometrica a
// dimetrica a trimetrica; scegliendo il piano attivo si decide su quale faccia
// del solido cade la forma che si disegna.
//
// Lo stato vive qui, in memoria, ed è letto sia dal pannello sia dal codice che
// costruisce i tracciati. Ogni pannello lo ripristina dalle proprie preferenze
// quando si apre, così ognuno dei due sistemi usa il proprio archivio senza che
// il resto del plug-in debba saperlo.

#ifdef __cplusplus
extern "C" {
#endif

/// Faccia sulla quale cadono rettangoli ed ellissi.
enum VSProjectionPlane {
	kVSPlaneTop = 0,    ///< piano orizzontale, fra i due assi inclinati
	kVSPlaneLeft = 1,   ///< faccia sinistra, fra l'asse sinistro e la verticale
	kVSPlaneRight = 2   ///< faccia destra, fra l'asse destro e la verticale
};

struct VSProjectionSettings {
	double leftAngle;   ///< gradi sull'orizzonte dell'asse sinistro
	double rightAngle;  ///< gradi sull'orizzonte dell'asse destro
	int plane;          ///< VSProjectionPlane
	int snapLine;       ///< la linea si aggancia all'asse più vicino
	double moveDistance; ///< distanza numerica per spostamento ed estrusione
	int moveAxis;        ///< VSProjectionAxis: X, Z oppure Y
	double scaleU;       ///< percentuale lungo il primo asse del piano
	double scaleV;       ///< percentuale lungo il secondo asse del piano
	double rotation;     ///< rotazione in gradi sul piano attivo
	double shear;        ///< inclinazione in gradi sul piano attivo
};

/// Impostazioni correnti. Alla prima chiamata restituisce l'isometrica 30/30.
VSProjectionSettings VSProjectionGet(void);

/// Sostituisce le impostazioni correnti, riportando gli angoli entro 1°–89°.
void VSProjectionSet(const VSProjectionSettings* settings);

/// Valori di partenza: isometrica, piano superiore, aggancio attivo.
VSProjectionSettings VSProjectionDefaults(void);

#ifdef __cplusplus
}
#endif

#endif
