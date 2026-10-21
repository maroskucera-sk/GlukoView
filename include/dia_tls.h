#pragma once

#include <WiFiClientSecure.h>

// tls_insecure=true  → setInsecure() len pre dáta (vlastný Nightscout so self-signed)
// tls_insecure=false → overenie cez ISRG Root X1 (Let's Encrypt a pod.)
// Aktualizácia firmvéru tento prepínač nepoužíva a certifikát overí vždy.
void configureTlsClient(WiFiClientSecure &client, bool tls_insecure);
