#ifndef VECTOR_SUITE_SELECTION_H
#define VECTOR_SUITE_SELECTION_H

// Un comando che lavora su oggetti interi deve ignorare sia i contenitori
// selezionati solo parzialmente sia i discendenti di un contenitore già
// selezionato per intero. In questo modo una selezione diretta dentro un gruppo
// non trascina con sé gli altri elementi del gruppo.
inline constexpr bool VSIsWholeObjectSelectionRoot(
	bool fullySelected,
	bool hasFullySelectedAncestor)
{
	return fullySelected && !hasFullySelectedAncestor;
}

#endif
