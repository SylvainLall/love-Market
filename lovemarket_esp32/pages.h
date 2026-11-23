#pragma once
#include <Arduino.h>
#include "model.h"

// Rendu des pages (équivalents des vues EJS)
String pageLogin(const String& error);
String pageDashboard(User& user, const String& error, const String& success);
String pageShop(User& user);
String pageMyServices(User& user);
String pageEditService(User& user, Service& service);
String pageNegotiations(User& user);
String pageRequestService(User& user);
String pageOfferService(User& user);
String pageAdmin(User& user, const String& importStatus);
String pageSpecialEvents(User& user, const String& error, const String& success);
String pageRelationshipHistory(User& user);
String pageGiftVoucher(const Voucher& voucher);
String page404();
