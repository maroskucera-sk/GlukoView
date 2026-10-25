#pragma once

#include <WString.h>

/** Kroky sťahovania. Do správy nejde meno, heslo, token ani adresa účtu. */
enum class DiaDiagStep : uint8_t {
    Boot = 0,
    Begin,
    Login,
    Connections,
    Graph,
    Discover,
    Refresh,
    Read,
    Result,
    Slow
};

void diaDiagBoot();
void diaDiagStep(int slot, DiaDiagStep step);
/** 0 = hodnota prišla. Inak FetchError ako číslo. */
void diaDiagResult(int slot, int errCode);
/** Slot drží sťahovanie. Druhý slot pôjde najbližšie prvý, aj po reštarte. */
void diaDiagSlow(int slot, uint32_t busyMs);
/** 0 alebo 1, ak minule niekto visel. Inak -1. Kľúč sa po prečítaní zmaže. */
int diaDiagTakeLead();
void diaDiagFormat(String &out);
