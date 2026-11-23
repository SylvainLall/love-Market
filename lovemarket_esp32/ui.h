#pragma once
#include <Arduino.h>

// Échappement HTML (équivalent <%= %> EJS)
String esc(const String& raw);

// Pastille de valeur en crédits (♥ 0..4) — « envie gratuite » si 0
String creditBadge(int credits);

// Options des catégories (stockées dans data.json, gérées dans l'admin)
String categoryOptions(const String& selected = "");

// Page complète avec layout (topbar + nav basse mobile + main) — layout.ejs
// Les badges de notifications sont calculés pour l'utilisateur courant
// (Ses envies / Nos échanges / Nos moments).
String renderPage(const String& title, const String& currentPath, const String& userId,
                  const String& userDisplayName, int userCredits, bool isAdmin,
                  const String& bodyHtml);

// Page autonome sans layout (login, 404)
String renderBarePage(const String& headHtml, const String& bodyHtml);

// CSS et favicon embarqués dans le firmware (aucun upload LittleFS requis)
extern const char LM_STYLE_CSS[];    // PROGMEM
extern const char LM_FAVICON_SVG[];  // PROGMEM
