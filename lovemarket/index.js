const express = require('express');
const session = require('express-session');
const path = require('path');
const fs = require('fs');

const app = express();
const PORT = process.env.PORT || 3000;
const DATA_FILE = path.join(__dirname, 'data.json');

// --- Default data ---
const defaultData = {
  users: [
    { id: 'Alice', password: '171088', role: 'user', displayName: 'Alice', credits: 120 },
    { id: 'Sylvain', password: '31051989', role: 'admin', displayName: 'Sylvain', credits: 120 },
  ],
  services: [
    {
      id: '1',
      sellerId: 'Sylvain',
      title: 'Nettoyage de la salle de bain',
      description: 'Salle de bain propre, miroir et lavabo nickels, sol lavé.',
      category: 'Maison',
      price: 25,
      icon: '🛁',
    },
    {
      id: '2',
      sellerId: 'Sylvain',
      title: 'Petit déjeuner au lit',
      description: 'Café ou thé, jus, viennoiserie et câlin du matin.',
      category: 'Romantique',
      price: 30,
      icon: '🥐',
    },
    {
      id: '3',
      sellerId: 'Sylvain',
      title: 'Soirée resto sans les enfants',
      description: 'Je m\'occupe de tout : réservation, garde des enfants, transport.',
      category: 'Sortie',
      price: 60,
      icon: '🍽️',
    },
    {
      id: '4',
      sellerId: 'Alice',
      title: 'Soirée série et plaid',
      description: 'Choix de la série, grignotage préparé, ambiance cosy sous le plaid.',
      category: 'Détente',
      price: 35,
      icon: '📺',
    },
    {
      id: '5',
      sellerId: 'Alice',
      title: 'Massage 30 minutes',
      description: 'Massage dos / nuque avec musique douce.',
      category: 'Détente',
      price: 40,
      icon: '💆',
    },
    {
      id: '6',
      sellerId: 'Alice',
      title: 'Garde des enfants pour ta soirée',
      description: 'Je m\'occupe des enfants pendant que tu profites de ta soirée.',
      category: 'Maison',
      price: 50,
      icon: '👶',
    },
  ],
  offers: [],
  giftVouchers: [],
  specialEvents: [],
  relationshipLevels: [
    { name: 'Coloc', minCredits: 0, icon: '🏠' },
    { name: 'Complice', minCredits: 100, icon: '🤝' },
    { name: 'Équipe', minCredits: 300, icon: '⚡' },
    { name: 'Connecté', minCredits: 600, icon: '💫' },
    { name: 'In Love', minCredits: 1000, icon: '💕' },
  ],
  nextServiceId: 7,
  nextOfferId: 1,
  nextVoucherId: 1,
  nextEventId: 1,
};

// --- Load data from file or use defaults ---
function loadData() {
  try {
    if (fs.existsSync(DATA_FILE)) {
      const fileData = fs.readFileSync(DATA_FILE, 'utf8');
      const data = JSON.parse(fileData);
      console.log('✅ Données chargées depuis data.json');
      return data;
    }
  } catch (error) {
    console.error('❌ Erreur lors du chargement des données:', error);
  }
  console.log('📝 Utilisation des données par défaut');
  return defaultData;
}

// --- Save data to file ---
function saveData() {
  try {
    const data = {
      users,
      services,
      offers,
      giftVouchers,
      specialEvents,
      relationshipLevels,
      nextServiceId,
      nextOfferId,
      nextVoucherId,
      nextEventId,
    };
    fs.writeFileSync(DATA_FILE, JSON.stringify(data, null, 2), 'utf8');
    console.log('💾 Données sauvegardées');
  } catch (error) {
    console.error('❌ Erreur lors de la sauvegarde:', error);
  }
}

// --- Initialize data ---
const loadedData = loadData();
const users = loadedData.users;
let services = loadedData.services;
let offers = loadedData.offers;
let giftVouchers = loadedData.giftVouchers;
let specialEvents = loadedData.specialEvents;
let relationshipLevels = loadedData.relationshipLevels;
let nextServiceId = loadedData.nextServiceId;
let nextOfferId = loadedData.nextOfferId;
let nextVoucherId = loadedData.nextVoucherId;
let nextEventId = loadedData.nextEventId;

// --- App setup ---
app.set('view engine', 'ejs');
app.set('views', path.join(__dirname, 'views'));

app.use(express.urlencoded({ extended: true }));
app.use(express.static(path.join(__dirname, 'public')));

app.use(
  session({
    secret: 'love-credits-secret',
    resave: false,
    saveUninitialized: false,
  })
);

function getCurrentUser(req) {
  if (!req.session.userId) return null;
  return users.find((u) => u.id === req.session.userId) || null;
}

function requireAuth(req, res, next) {
  const user = getCurrentUser(req);
  if (!user) {
    return res.redirect('/login');
  }
  req.user = user;
  next();
}

function requireAdmin(req, res, next) {
  const user = getCurrentUser(req);
  if (!user || user.role !== 'admin') {
    return res.status(403).send('Forbidden');
  }
  req.user = user;
  next();
}

// NEW: Helper to count unread notifications
function getPendingNotificationsCount(userId) {
  return offers.filter((o) => {
    // Count if user is involved AND hasn't read the current status
    const isInvolved = o.fromUserId === userId || o.toUserId === userId;
    const hasNotRead = !o.readBy || !o.readBy.includes(userId);
    
    // Exclude offers that are still pending and user is the recipient (already visible on dashboard)
    // Only notify for status changes: accepted, rejected, countered, realized
    const isStatusChange = ['accepted', 'rejected', 'countered', 'realized'].includes(o.status);
    
    // Also count pending offers where user is the recipient (toUserId)
    const isPendingForMe = o.status === 'pending' && o.toUserId === userId;
    
    return isInvolved && hasNotRead && (isStatusChange || isPendingForMe);
  }).length;
}

// NEW: Calculate relationship level based on credit exchanges in last 30 days
function getRelationshipLevel() {
  const thirtyDaysAgo = new Date();
  thirtyDaysAgo.setDate(thirtyDaysAgo.getDate() - 30);
  
  // Count credits exchanged in accepted/realized offers from last 30 days
  const totalCredits = offers
    .filter((o) => {
      if (!o.createdAt) return false;
      const offerDate = new Date(o.createdAt);
      return offerDate >= thirtyDaysAgo && (o.status === 'accepted' || o.status === 'realized');
    })
    .reduce((sum, o) => {
      // Count base price + super service bonus if applicable
      let credits = o.offeredPrice || 0;
      if (o.isSuperService && o.superServiceBonus) {
        credits += o.superServiceBonus;
      }
      return sum + credits;
    }, 0);
  
  // Find the highest level achieved
  let currentLevel = relationshipLevels[0];
  for (let i = relationshipLevels.length - 1; i >= 0; i--) {
    if (totalCredits >= relationshipLevels[i].minCredits) {
      currentLevel = relationshipLevels[i];
      break;
    }
  }
  
  return {
    level: currentLevel,
    totalCredits,
    nextLevel: relationshipLevels[relationshipLevels.indexOf(currentLevel) + 1] || null,
  };
}

// --- Routes ---

// NEW: Get transaction history for relationship level
app.get('/relationship-history', requireAuth, (req, res) => {
  const user = req.user;
  
  // Get all accepted/received offers in the last 30 days for relationship level calculation
  const thirtyDaysAgo = new Date();
  thirtyDaysAgo.setDate(thirtyDaysAgo.getDate() - 30);
  
  const transactionHistory = offers
    .filter((o) => {
      if (!o.createdAt) return false;
      const offerDate = new Date(o.createdAt);
      return offerDate >= thirtyDaysAgo && (o.status === 'accepted' || o.status === 'realized');
    })
    .map((o) => {
      const service = services.find((s) => s.id === o.serviceId);
      const fromUser = users.find((u) => u.id === o.fromUserId);
      const toUser = users.find((u) => u.id === o.toUserId);
      
      let credits = o.offeredPrice || 0;
      if (o.isSuperService && o.superServiceBonus) {
        credits += o.superServiceBonus;
      }
      
      return {
        ...o,
        service,
        fromUser,
        toUser,
        totalCredits: credits,
        date: new Date(o.createdAt).toLocaleDateString('fr-FR', {
          day: '2-digit',
          month: '2-digit',
          year: 'numeric',
          hour: '2-digit',
          minute: '2-digit'
        })
      };
    })
    .sort((a, b) => new Date(b.createdAt) - new Date(a.createdAt)); // Sort by most recent first
  
  const relationshipData = getRelationshipLevel();
  
  res.render('relationship-history', {
    user,
    transactionHistory,
    relationshipData,
    notificationCount: getPendingNotificationsCount(user.id),
    currentPath: '/relationship-history',
  });
});

app.get('/', (req, res) => {
  const user = getCurrentUser(req);
  if (!user) return res.redirect('/login');
  res.redirect('/dashboard');
});

app.get('/login', (req, res) => {
  const user = getCurrentUser(req);
  if (user) return res.redirect('/dashboard');
  res.render('login', { error: null, title: 'Connexion', user: null });
});

app.post('/login', (req, res) => {
  const { userId, password } = req.body;
  const user = users.find((u) => u.id === userId && u.password === password);
  if (!user) {
    return res.render('login', { error: 'Identifiants incorrects', title: 'Connexion', user: null });
  }
  req.session.userId = user.id;
  res.redirect('/dashboard');
});

app.get('/logout', (req, res) => {
  req.session.destroy(() => {
    res.redirect('/login');
  });
});

app.get('/dashboard', requireAuth, (req, res) => {
  const user = req.user;
  const partner = users.find((u) => u.id !== user.id);

  const myServices = services.filter((s) => s.sellerId === user.id);
  const partnerServices = services.filter((s) => s.sellerId === partner.id);
  const myOffers = offers.filter((o) => o.toUserId === user.id && o.status === 'pending');

  // NEW: Find purchases of MY services by partner (pending approval)
  const myServicesPurchases = offers.filter(
    (o) => o.toUserId === user.id && 
           o.type === 'purchase' && 
           o.status === 'pending'
  ).map((o) => {
    const service = services.find((s) => s.id === o.serviceId);
    const buyer = users.find((u) => u.id === o.fromUserId);
    return { ...o, service, buyer };
  });

  // NEW: Find custom requests for MY services
  const myServicesRequests = offers.filter(
    (o) => o.toUserId === user.id && 
           o.type === 'request' && 
           o.status === 'pending'
  ).map((o) => {
    const buyer = users.find((u) => u.id === o.fromUserId);
    return { ...o, buyer };
  });

  // NEW: Get success/error messages from session
  const error = req.session.error;
  const success = req.session.success;
  delete req.session.error;
  delete req.session.success;
  
  // NEW: Get relationship level
  const relationshipData = getRelationshipLevel();

  res.render('dashboard', {
    user,
    partner,
    myServicesCount: myServices.length,
    partnerServicesCount: partnerServices.length,
    pendingOffers: myOffers,
    myServicesPurchases, // NEW
    myServicesRequests, // NEW
    notificationCount: getPendingNotificationsCount(user.id), // NEW
    currentPath: '/dashboard',
    error, // NEW
    success, // NEW
    relationshipData, // NEW
  });
});

app.get('/shop', requireAuth, (req, res) => {
  const user = req.user;
  const partner = users.find((u) => u.id !== user.id);
  const partnerServices = services.filter((s) => s.sellerId === partner.id);

  res.render('shop', { 
    user, 
    partner, 
    services: partnerServices,
    notificationCount: getPendingNotificationsCount(user.id),
    currentPath: '/shop',
  });
});

app.get('/my-services', requireAuth, (req, res) => {
  const user = req.user;
  const myServices = services.filter((s) => s.sellerId === user.id);

  res.render('my-services', { 
    user, 
    services: myServices,
    notificationCount: getPendingNotificationsCount(user.id),
    currentPath: '/my-services',
  });
});

app.post('/services', requireAuth, (req, res) => {
  const user = req.user;
  const { title, description, price, category, icon } = req.body;
  if (!title || !price) {
    return res.redirect('/my-services');
  }

  services.push({
    id: String(nextServiceId++),
    sellerId: user.id,
    title,
    description: description || '',
    category: category || 'Romantique',
    price: Number(price) || 0,
    icon: icon || '💗',
  });

  saveData();
  res.redirect('/my-services');
});

app.get('/services/:id/edit', requireAuth, (req, res) => {
  console.log('[DEBUG] GET /services/:id/edit called with id=', req.params.id);
  const user = req.user;
  const service = services.find((s) => s.id === req.params.id && s.sellerId === user.id);
  if (!service) {
    console.log('[DEBUG] Service not found or not owned by user, redirecting to /my-services');
    return res.redirect('/my-services');
  }

  res.render('edit-service', { 
    user, 
    service,
    notificationCount: getPendingNotificationsCount(user.id),
    currentPath: '/my-services', // NEW: Add currentPath
  });
});

app.post('/services/:id/edit', requireAuth, (req, res) => {
  const user = req.user;
  const service = services.find((s) => s.id === req.params.id && s.sellerId === user.id);
  if (!service) return res.redirect('/my-services');

  const { title, description, price, category, icon } = req.body;
  service.title = title || service.title;
  service.description = description || '';
  service.category = category || service.category;
  service.price = Number(price) || 0;
  service.icon = icon || service.icon || '💗';

  saveData();
  res.redirect('/my-services');
});

app.post('/services/:id/delete', requireAuth, (req, res) => {
  const user = req.user;
  const index = services.findIndex((s) => s.id === req.params.id && s.sellerId === user.id);
  if (index === -1) return res.redirect('/my-services');

  services.splice(index, 1);
  saveData();
  res.redirect('/my-services');
});

app.post('/services/:id/buy', requireAuth, (req, res) => {
  const user = req.user;
  const service = services.find((s) => s.id === req.params.id);
  if (!service) return res.redirect('/shop');

  const partner = users.find((u) => u.id === service.sellerId);
  if (!partner) return res.redirect('/shop');

  if (user.credits < service.price) {
    return res.redirect('/shop');
  }

  // Debit buyer immediately
  user.credits -= service.price;

  // Create purchase request (type: 'purchase')
  offers.push({
    id: String(nextOfferId++),
    serviceId: service.id,
    fromUserId: user.id,
    toUserId: partner.id,
    offeredPrice: service.price,
    comment: '',
    status: 'pending',
    type: 'purchase', // NEW: distinguish from negotiation
    debitedAmount: service.price, // Track debited amount for refund
  });

  saveData();
  res.redirect('/dashboard');
});

app.post('/services/:id/offer', requireAuth, (req, res) => {
  const user = req.user;
  const { offeredPrice, comment } = req.body;
  const service = services.find((s) => s.id === req.params.id);
  if (!service) return res.redirect('/shop');

  const toUserId = service.sellerId;

  offers.push({
    id: String(nextOfferId++),
    serviceId: service.id,
    fromUserId: user.id,
    toUserId,
    offeredPrice: Number(offeredPrice) || 0,
    comment: comment || '',
    status: 'pending',
    type: 'negotiation', // NEW: distinguish from purchase
  });

  saveData();
  res.redirect('/dashboard');
});

app.get('/negotiations', requireAuth, (req, res) => {
  const user = req.user;
  const relatedOffers = offers.filter(
    (o) => o.fromUserId === user.id || o.toUserId === user.id
  );

  // NEW: Mark all related offers as read by current user
  relatedOffers.forEach((o) => {
    if (!o.readBy) {
      o.readBy = [];
    }
    if (!o.readBy.includes(user.id)) {
      o.readBy.push(user.id);
    }
  });

  const enriched = relatedOffers.map((o) => {
    const service = services.find((s) => s.id === o.serviceId);
    const fromUser = users.find((u) => u.id === o.fromUserId);
    const toUser = users.find((u) => u.id === o.toUserId);
    return { ...o, service, fromUser, toUser };
  });

  // NEW: Sort by ID descending (most recent first)
  enriched.sort((a, b) => parseInt(b.id) - parseInt(a.id));

  res.render('negotiations', { 
    user, 
    offers: enriched,
    notificationCount: getPendingNotificationsCount(user.id),
    currentPath: '/negotiations',
  });
});

// NEW: Request a custom service
app.get('/request-service', requireAuth, (req, res) => {
  const user = req.user;
  const partner = users.find((u) => u.id !== user.id);
  res.render('request-service', { 
    user, 
    partner,
    notificationCount: getPendingNotificationsCount(user.id),
    currentPath: '/request-service',
  });
});

app.post('/request-service', requireAuth, (req, res) => {
  const user = req.user;
  const partner = users.find((u) => u.id !== user.id);
  const { title, description, offeredPrice, category, icon, isSuperService } = req.body;

  if (!title || !offeredPrice) {
    return res.redirect('/request-service');
  }
  
  // NEW: Calculate super service bonus
  let bonusCredits = 0;
  if (isSuperService === 'true') {
    bonusCredits = Math.round(Number(offeredPrice) * 0.3);
  }

  offers.push({
    id: String(nextOfferId++),
    serviceId: null, // No existing service
    fromUserId: user.id,
    toUserId: partner.id,
    offeredPrice: Number(offeredPrice) || 0,
    comment: description || '',
    status: 'pending',
    type: 'request', // NEW: custom service request (you ask your partner to do something)
    serviceTitle: title,
    serviceCategory: category || 'Romantique',
    serviceIcon: icon || '💗',
    originalRequesterId: user.id, // NEW: Track who originally requested the service (payer)
    isSuperService: isSuperService === 'true',
    superServiceBonus: bonusCredits,
  });

  saveData();
  res.redirect('/negotiations');
});

// NEW: Offer a service to earn credits (you propose something to your partner)
app.get('/offer-service', requireAuth, (req, res) => {
  const user = req.user;
  const partner = users.find((u) => u.id !== user.id);
  res.render('offer-service', {
    user,
    partner,
    notificationCount: getPendingNotificationsCount(user.id),
    currentPath: '/offer-service',
  });
});

app.post('/offer-service', requireAuth, (req, res) => {
  const user = req.user;
  const partner = users.find((u) => u.id !== user.id);
  const { title, description, offeredPrice, category, icon, isSuperService } = req.body;

  if (!title || !offeredPrice) {
    return res.redirect('/offer-service');
  }
  
  // NEW: Calculate super service bonus
  let bonusCredits = 0;
  if (isSuperService === 'true') {
    bonusCredits = Math.round(Number(offeredPrice) * 0.3);
  }

  // Here, you propose a service and your partner will pay if accepted
  offers.push({
    id: String(nextOfferId++),
    serviceId: null,
    fromUserId: user.id,
    toUserId: partner.id,
    offeredPrice: Number(offeredPrice) || 0,
    comment: description || '',
    status: 'pending',
    type: 'request', // Reuse 'request' type, but payer will be originalRequesterId (partner)
    serviceTitle: title,
    serviceCategory: category || 'Romantique',
    serviceIcon: icon || '💗',
    originalRequesterId: partner.id, // Partner will pay if they accept
    isSuperService: isSuperService === 'true',
    superServiceBonus: bonusCredits,
  });

  saveData();
  res.redirect('/negotiations');
});

app.post('/offers/:id/respond', requireAuth, (req, res) => {
  const user = req.user;
  const { action, counterPrice, counterComment } = req.body; // NEW: counter-offer params
  const offer = offers.find((o) => o.id === req.params.id);
  if (!offer || offer.toUserId !== user.id || offer.status !== 'pending') {
    return res.redirect('/negotiations');
  }

  if (action === 'accept') {
    const service = services.find((s) => s.id === offer.serviceId);
    const buyer = users.find((u) => u.id === offer.fromUserId);
    const seller = users.find((u) => u.id === offer.toUserId);

    if (offer.type === 'purchase') {
      // For purchase: credits already debited, just credit seller
      if (service && buyer && seller) {
        seller.credits += offer.offeredPrice;
        offer.status = 'accepted';
        // NEW: Reset readBy so both users see the status change
        offer.readBy = [user.id]; // Only the one who accepted has read it
      }
    } else if (offer.type === 'request') {
      // For custom service request: find original requester and debit them
      const originalRequester = users.find((u) => u.id === (offer.originalRequesterId || offer.fromUserId));
      const serviceProvider = users.find((u) => u.id !== originalRequester.id);
      
      if (originalRequester && serviceProvider && originalRequester.credits >= offer.offeredPrice) {
        originalRequester.credits -= offer.offeredPrice;
        serviceProvider.credits += offer.offeredPrice;
        
        // NEW: Add super service bonus
        if (offer.isSuperService && offer.superServiceBonus > 0) {
          serviceProvider.credits += offer.superServiceBonus;
        }
        
        offer.status = 'accepted';
        // NEW: Reset readBy so both users see the status change
        offer.readBy = [user.id]; // Only the one who accepted has read it
      }
    } else {
      // For negotiation: debit buyer and credit seller
      if (service && buyer && seller && buyer.credits >= offer.offeredPrice) {
        buyer.credits -= offer.offeredPrice;
        seller.credits += offer.offeredPrice;
        
        // NEW: Add super service bonus for service provider
        if (offer.isSuperService && offer.superServiceBonus > 0) {
          seller.credits += offer.superServiceBonus;
        }
        
        offer.status = 'accepted';
        // NEW: Reset readBy so both users see the status change
        offer.readBy = [user.id]; // Only the one who accepted has read it
      }
    }
  } else if (action === 'counter') {
    // NEW: Counter-offer
    const buyer = users.find((u) => u.id === offer.fromUserId);
    const seller = users.find((u) => u.id === offer.toUserId);

    if (counterPrice && seller && buyer) {
      // Find root offer ID (for grouping)
      const rootOfferId = offer.parentOfferId || offer.id;
      
      offers.push({
        id: String(nextOfferId++),
        serviceId: offer.serviceId,
        fromUserId: seller.id, // Seller makes counter-offer
        toUserId: buyer.id,
        offeredPrice: Number(counterPrice) || 0,
        comment: counterComment || 'Contre-proposition',
        status: 'pending',
        type: offer.type === 'request' ? 'request' : 'negotiation',
        serviceTitle: offer.serviceTitle,
        serviceCategory: offer.serviceCategory,
        serviceIcon: offer.serviceIcon,
        parentOfferId: offer.id, // Link to previous offer in chain
        rootOfferId: rootOfferId, // NEW: Link to first offer in chain for grouping
        originalRequesterId: offer.originalRequesterId || offer.fromUserId, // NEW: Track original requester
      });
      offer.status = 'countered';
      // NEW: Reset readBy so the recipient sees the counter-offer
      offer.readBy = [user.id]; // Only the one who countered has read it
    }
  } else {
    // Reject: refund if it was a purchase
    if (offer.type === 'purchase' && offer.debitedAmount) {
      const buyer = users.find((u) => u.id === offer.fromUserId);
      if (buyer) {
        buyer.credits += offer.debitedAmount;
      }
    }
    offer.status = 'rejected';
    // NEW: Reset readBy so the requester sees the rejection
    offer.readBy = [user.id]; // Only the one who rejected has read it
  }

  saveData();
  res.redirect('/negotiations');
});

app.post('/offers/:id/realize', requireAuth, (req, res) => {
  const user = req.user;
  const { feedback } = req.body; // NEW: Get feedback rating
  const offer = offers.find((o) => o.id === req.params.id);
  if (!offer || offer.status !== 'accepted') {
    return res.redirect('/negotiations');
  }

  // Determine buyer (the one who used credits)
  let buyerId;
  if (offer.type === 'purchase') {
    buyerId = offer.fromUserId;
  } else if (offer.type === 'request') {
    buyerId = offer.originalRequesterId || offer.fromUserId;
  } else {
    buyerId = offer.fromUserId;
  }

  if (user.id !== buyerId) {
    return res.redirect('/negotiations');
  }

  offer.status = 'realized';
  offer.readBy = [user.id];
  
  // NEW: Store feedback
  if (feedback) {
    offer.feedback = feedback; // 'happy', 'neutral', 'sad'
    offer.feedbackAt = new Date().toISOString();
  }

  saveData();
  res.redirect('/negotiations');
});

// NEW: Credit donation
app.post('/donate-credits', requireAuth, (req, res) => {
  const user = req.user;
  const partner = users.find((u) => u.id !== user.id);
  const { amount, message } = req.body;
  
  const amountNum = Number(amount);
  
  if (isNaN(amountNum) || amountNum <= 0 || amountNum > user.credits) {
    req.session.error = 'Montant invalide ou insuffisant';
    return res.redirect('/dashboard');
  }
  
  // Transfer credits
  user.credits -= amountNum;
  partner.credits += amountNum;
  
  // Create a donation record in offers
  const donation = {
    id: String(nextOfferId++),
    serviceId: null,
    fromUserId: user.id,
    toUserId: partner.id,
    offeredPrice: amountNum,
    comment: message || 'Don de crédits',
    status: 'accepted', // Donation is immediately accepted
    type: 'donation',
    serviceTitle: 'Don de crédits',
    serviceCategory: 'Cadeau',
    serviceIcon: '💝',
    originalRequesterId: user.id,
    createdAt: new Date().toISOString(),
  };
  
  offers.push(donation);
  
  saveData();
  req.session.success = `Don de ${amountNum} ❤️ crédits effectué à ${partner.displayName} !`;
  res.redirect('/dashboard');
});

app.get('/admin', requireAdmin, (req, res) => {
  const user = req.user;
  res.render('admin', { 
    user, 
    users,
    giftVouchers,
    specialEvents, // NEW: Pass special events to admin view
    relationshipLevels, // NEW: Pass relationship levels
    notificationCount: getPendingNotificationsCount(user.id),
    currentPath: '/admin',
  });
});

// NEW: Update relationship level thresholds
app.post('/admin/relationship-levels', requireAdmin, (req, res) => {
  const { levelIndex, minCredits } = req.body;
  const index = Number(levelIndex);
  const credits = Number(minCredits);
  
  if (index >= 0 && index < relationshipLevels.length && credits >= 0) {
    relationshipLevels[index].minCredits = credits;
    saveData();
  }
  
  res.redirect('/admin');
});

// NEW: Create special event
app.post('/admin/special-event', requireAdmin, (req, res) => {
  const { title, description, credits, targetUser } = req.body;
  
  const creditsNum = Number(credits);
  
  if (!title || !description || isNaN(creditsNum) || creditsNum <= 0) {
    return res.redirect('/admin');
  }
  
  const event = {
    id: String(nextEventId++),
    title,
    description,
    credits: creditsNum,
    targetUser: targetUser || 'both', // 'both', 'Alice', 'Sylvain'
    status: 'active', // 'active', 'completed', 'cancelled'
    createdAt: new Date().toISOString(),
    assignedTo: null, // Will be assigned when user accepts
    completedBy: null,
    completedAt: null,
  };
  
  specialEvents.push(event);
  
  saveData();
  res.redirect('/admin');
});

// NEW: Get special events for user
app.get('/special-events', requireAuth, (req, res) => {
  const user = req.user;
  
  // Get active events that are for this user or for both users
  const userEvents = specialEvents.filter(event => {
    return event.status === 'active' && 
           (event.targetUser === 'both' || event.targetUser === user.id);
  });
  
  res.render('special-events', {
    user,
    events: userEvents,
    notificationCount: getPendingNotificationsCount(user.id),
    currentPath: '/special-events',
  });
});

// NEW: Accept special event
app.post('/special-events/:id/accept', requireAuth, (req, res) => {
  const user = req.user;
  const eventId = req.params.id;
  
  const event = specialEvents.find(e => e.id === eventId && e.status === 'active');
  
  if (!event) {
    return res.redirect('/special-events');
  }
  
  // Check if event is for this user or for both
  if (event.targetUser !== 'both' && event.targetUser !== user.id) {
    return res.redirect('/special-events');
  }
  
  // Check if event is already assigned
  if (event.assignedTo) {
    req.session.error = 'Cet événement est déjà attribué à quelqu\'un d\'autre.';
    return res.redirect('/special-events');
  }
  
  // Assign the event to this user
  event.assignedTo = user.id;
  
  // Create an offer record for tracking
  const offer = {
    id: String(nextOfferId++),
    serviceId: null,
    fromUserId: user.id,
    toUserId: user.id, // Same user for special events
    offeredPrice: event.credits,
    comment: `Événement spécial: ${event.title}`,
    status: 'accepted', // Will be completed when user marks as done
    type: 'special_event',
    serviceTitle: event.title,
    serviceCategory: 'Événement spécial',
    serviceIcon: '🎯',
    originalRequesterId: user.id,
    eventId: event.id, // Link to the special event
    createdAt: new Date().toISOString(),
  };
  
  offers.push(offer);
  
  saveData();
  req.session.success = `Vous avez accepté l'événement: ${event.title}`;
  res.redirect('/special-events');
});

// NEW: Mark special event as completed
app.post('/special-events/:id/complete', requireAuth, (req, res) => {
  const user = req.user;
  const eventId = req.params.id;
  
  const event = specialEvents.find(e => e.id === eventId && e.status === 'active');
  const offer = offers.find(o => o.eventId === eventId && o.fromUserId === user.id);
  
  if (!event || !offer || event.assignedTo !== user.id) {
    return res.redirect('/special-events');
  }
  
  // Award credits to the user
  user.credits += event.credits;
  
  // Mark event as completed
  event.completedBy = user.id;
  event.completedAt = new Date().toISOString();
  event.status = 'completed';
  
  // Update the offer status
  offer.status = 'realized';
  
  saveData();
  req.session.success = `Félicitations ! Vous avez reçu ${event.credits} ❤️ crédits pour avoir complété l'événement: ${event.title}`;
  res.redirect('/special-events');
});

// NEW: Generate gift voucher
app.post('/admin/gift-voucher', requireAdmin, (req, res) => {
  const { credits, comment } = req.body;
  const creditsNum = Number(credits);
  
  if (isNaN(creditsNum) || creditsNum <= 0) {
    return res.redirect('/admin');
  }
  
  // Generate unique 4-digit code
  let code;
  let isUnique = false;
  
  while (!isUnique) {
    code = Math.floor(1000 + Math.random() * 9000).toString();
    isUnique = !giftVouchers.some(v => v.code === code);
  }
  
  const voucher = {
    id: String(nextVoucherId++),
    code,
    credits: creditsNum,
    comment: comment || '',
    used: false,
    usedBy: null,
    createdAt: new Date().toISOString(),
  };
  
  giftVouchers.push(voucher);
  
  saveData();
  res.redirect('/admin');
});

// NEW: Get printable voucher
app.get('/gift-voucher/:code', (req, res) => {
  const voucher = giftVouchers.find(v => v.code === req.params.code);
  
  if (!voucher || voucher.used) {
    return res.status(404).send('Voucher not found or already used');
  }
  
  res.render('gift-voucher', { voucher });
});

// NEW: Redeem gift voucher
app.post('/redeem-voucher', requireAuth, (req, res) => {
  const user = req.user;
  const { code } = req.body;
  
  const voucher = giftVouchers.find(v => v.code === code && !v.used);
  
  if (!voucher) {
    req.session.error = 'Code invalide ou déjà utilisé';
    return res.redirect('/dashboard');
  }
  
  // Apply credits
  user.credits += voucher.credits;
  voucher.used = true;
  voucher.usedBy = user.id;
  voucher.usedAt = new Date().toISOString();
  
  saveData();
  req.session.success = `Félicitations ! Vous avez reçu ${voucher.credits} ❤️ crédits !`;
  res.redirect('/dashboard');
});

app.post('/admin/credits', requireAdmin, (req, res) => {
  const { targetId, credits } = req.body;
  const target = users.find((u) => u.id === targetId);
  if (target && !Number.isNaN(Number(credits))) {
    target.credits = Number(credits);
    saveData();
  }
  res.redirect('/admin');
});

app.listen(PORT, () => {
  console.log(`Love market app listening on http://localhost:${PORT}`);
  console.log('[DEBUG] Route /services/:id/edit is registered.');
});
