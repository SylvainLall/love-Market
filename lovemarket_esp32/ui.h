#pragma once
#include <Arduino.h>

// Échappement HTML (équivalent <%= %> EJS)
String esc(const String& raw);

// Options des <select> catégories / icônes (liste identique aux vues EJS)
String categoryOptions(const String& selected = "");
String iconOptions(const String& selected = "");

// Page complète avec layout (topbar + nav + main) — équivalent layout.ejs
// showEventsLink : les pages dashboard / relationship-history / special-events
// affichent aussi le lien "Événements" dans la nav (comme l'original)
// notificationCount : badge affiché sur le lien Négociations (0 = pas de badge)
String renderPage(const String& title, const String& currentPath, bool showEventsLink,
                  const String& bodyHtml, const String& userDisplayName,
                  int userCredits, bool isAdmin, int notificationCount);

// Page autonome sans layout (login, bon cadeau imprimable)
String renderBarePage(const String& headHtml, const String& bodyHtml);

// CSS et favicon embarqués dans le firmware (aucun upload LittleFS requis)
extern const char LM_STYLE_CSS[];    // PROGMEM
extern const char LM_FAVICON_SVG[];  // PROGMEM
