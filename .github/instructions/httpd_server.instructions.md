---
description: "Règles de développement C/C++ pour pick_httpd_server. Utilisé pour créer ou modifier des fichiers .cpp ou .h du serveur HTTP : conventions de codage, gestion des erreurs, configuration libconfig, routage URL, intégration microhttpd/pcre2/OpenQM."
applyTo: ["**/*.cpp", "**/*.h", "**/Makefile"]
---

# Instructions de développement – pick_httpd_server

## Présentation du projet

`pick_httpd_server` est un serveur HTTP léger en C/C++ qui :
- Reçoit des requêtes HTTP via `libmicrohttpd`.
- Valide les URLs, les méthodes HTTP et les paramètres GET avec des expressions régulières (`pcre2`).
- Appelle des routines OpenQM (Pick Basic) via l'API OpenQM.
- Construit la réponse HTTP depuis les paramètres retournés par la routine.

Il n'est **jamais exposé directement à Internet** : il est placé derrière Apache httpd en reverse proxy.

## Conventions de codage C/C++

### Général
- Standard C++ : **C++17** minimum.
- Compilation : `g++` avec options `-Wall -Wextra -std=c++17`.
- Chaque fichier source (`.cpp`) a son en-tête correspondant (`.h`).
- Les fichiers `.h` doivent utiliser des include guards (`#pragma once` ou `#ifndef`).

### Nommage
- Classes : `PascalCase` (ex. `HttpdServer`, `RequestHandler`).
- Fonctions et méthodes : `snake_case` (ex. `handle_request`, `parse_config`).
- Variables : `snake_case`, noms explicites — pas de variables à une lettre sauf index de boucle court.
- Constantes : `UPPER_SNAKE_CASE`.
- Membres de classe : préfixe `m_` (ex. `m_port`, `m_config`).

### Indentation et formatage
- Indentation : 4 espaces (pas de tabulations).
- Accolades ouvrantes sur la même ligne que la déclaration (`K&R style`).
- Lignes ≤ 120 caractères.

### Commentaires
- En-tête de fichier : description, auteur, date.
- En-tête de fonction : description, paramètres (in/out), valeur de retour.
- Commentaires en ligne en français pour les parties non triviales.

## Gestion des erreurs
- Vérifier **systématiquement** le code de retour de toutes les fonctions système et bibliothèques.
- En cas d'erreur critique, logger et terminer proprement (pas de crash sans message).
- Utiliser le système de logs défini dans `pick_httpd_server_logs.cpp/.h`.
- Ne pas exposer d'informations système dans les réponses HTTP d'erreur.

## Configuration (libconfig)
- Le fichier de configuration suit la syntaxe libconfig.
- Structure : sections `httpd`, `openqm`, `url` (voir README.md).
- Valider la configuration au démarrage ; refuser de démarrer si elle est invalide.
- Ne jamais hardcoder des valeurs qui doivent être dans la configuration.

## Routage URL
- La correspondance URL → routine OpenQM est **entièrement dans la configuration**, pas dans le code.
- Les segments d'URL peuvent être des chaînes fixes (`path`) ou des expressions régulières (`pattern`).
- Les méthodes HTTP acceptées et les paramètres GET valides sont déclarés dans la configuration.

## Sécurité
- Valider et assainir toutes les entrées (URL, paramètres, en-têtes) avant de les passer à OpenQM.
- Ne pas permettre de path traversal dans les URLs (Une attaque de traversée de chemin — path traversal ou directory traversal — consiste à glisser dans une URL des séquences comme .. ou %2e%2e%2f pour remonter dans l'arborescence et atteindre des ressources non prévues, exemple : /api/../../etc/passwd).
- Limiter la taille des requêtes acceptées.
- Les erreurs de validation retournent le statut HTTP approprié (400, 404, 405) avec un corps JSON structuré.

## Dépendances
| Bibliothèque | Usage |
|-------------|-------|
| `libmicrohttpd` | Serveur HTTP |
| `pcre2` | Expressions régulières pour la validation d'URL |
| `libconfig` | Parsing du fichier de configuration |
| OpenQM API | Appel des routines Pick Basic |
