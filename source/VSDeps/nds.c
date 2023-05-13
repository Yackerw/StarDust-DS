#include "nds.h"

u16(*keysCallback)();

u16 prevKeysState;
u16 currKeysState;

void scanKeys() {
	if (keysCallback != NULL) {
		prevKeysState = currKeysState;
		currKeysState = keysCallback();
	}
}

u16 keysDown() {
	return currKeysState & ~prevKeysState;
}

u16 keysHeld() {
	return currKeysState;
}

u16 keysCurrent() {
	return currKeysState;
}

u16 keysUp() {
	return prevKeysState & ~currKeysState;
}