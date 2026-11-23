#pragma once
#include <Arduino.h>
#include "model.h"

// Rendu des pages V1 (équivalent des vues EJS)
String pageLogin(const String& error);
String pageHome(User& user, const String& error, const String& success); // accueil chaleureux
String pageMyWishes(User& user);
String pageWishForm(User& user, Wish* wish); // wish = nullptr pour une création
String pagePartnerWishes(User& user);
String pageExchanges(User& user);
String pageMoments(User& user);
String pageAdmin(User& user, const String& importStatus);
String page404();
