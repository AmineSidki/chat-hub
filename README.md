# Chat Hub — Application de Chat Centralisée

Application de chat en temps réel en architecture **client–serveur**, développée en **C** avec sockets TCP et threads POSIX.

## Table des matières

1. [Dépendances et compilation](#1-dépendances-et-compilation)
2. [Instructions d'exécution](#2-instructions-dexécution)
3. [Protocole de communication](#3-protocole-de-communication)
4. [Choix techniques et difficultés](#4-choix-techniques-et-difficultés)
5. [Commandes disponibles](#5-commandes-disponibles)

## 1. Dépendances et compilation

### Dépendances

| Dépendance | Description |
|---|---|
| `gcc` | Compilateur C (GCC 9+ recommandé) |
| `pthread` | Bibliothèque POSIX threads (incluse dans glibc) |

Sur **Ubuntu/Debian** :
```bash
sudo apt update && sudo apt install gcc build-essential
```

Sur **Arch Linux** :
```bash
sudo pacman -S gcc
```

### Compilation

**Avec GCC directement :**
```bash
# Compiler le serveur
gcc -o chat_server server.c -lpthread

# Compiler le client
gcc -o chat_client client.c -lpthread
```

**Avec le Makefile (recommandé) :**
```bash
make all
```

## 2. Instructions d'exécution

### Démarrer le serveur

```bash
./chat_server <port>
```

Exemple :
```bash
./chat_server 5555
```

Sortie attendue :
```
Server listening on port 5555...
```

### Démarrer un client

```bash
./chat_client <adresse_IP_serveur> <port>
```

Exemples :
```bash
# Connexion locale
./chat_client 127.0.0.1 5555

# Connexion réseau
./chat_client 192.168.1.10 5555
```

### Scénario d'utilisation complet

**Terminal 1 (Serveur) :**
```
$ ./chat_server 5555
Server listening on port 5555...
Client connected: Kamal (127.0.0.1)
Client connected: Said (127.0.0.1)
Client disconnected: Said
```

**Terminal 2 (Client 1) :**
```
$ ./chat_client 127.0.0.1 5555
Enter your username: Kamal
*** Said joined the chat ***
[Said]: Bonjour Kamal !
You: Bonjour tout le monde !
```

**Terminal 3 (Client 2) :**
```
$ ./chat_client 127.0.0.1 5555
Enter your username: Said
*** Kamal joined the chat ***
[Kamal]: Bonjour tout le monde !
You: /msg Kamal Salut, comment ça va ?
You: /quit
```

## 3. Protocole de communication

### Transport

- **Protocole** : TCP/IP (connexion persistante, fiable, ordonnée)
- **Port par défaut** : configurable au lancement (ex: `5555`)

### Format des messages

Tous les messages sont transmis en **texte brut UTF-8**, terminés par `\n`.

| Type | Format sur le réseau | Exemple |
|---|---|---|
| Enregistrement pseudo | `<username>\n` | `Kamal\n` |
| Message public | `<texte>\n` | `Bonjour tout le monde !\n` |
| Message privé | `/msg <pseudo> <texte>\n` | `/msg Said Salut !\n` |
| Quitter | `/quit\n` | `/quit\n` |
| Lister les utilisateurs | `/users\n` | `/users\n` |

### Messages système émis par le serveur

| Événement | Message envoyé aux clients |
|---|---|
| Arrivée d'un utilisateur | `*** <pseudo> joined the chat ***` |
| Départ d'un utilisateur | `*** <pseudo> left the chat ***` |
| Pseudo déjà pris | `ERROR: Username already taken. Disconnecting...` |
| Message public reçu | `[<pseudo>]: <texte>` |
| Message privé reçu | `[PM from <pseudo>]: <texte>` |

### Séquence de connexion

```
Client                          Serveur
  |------- TCP connect() --------->|
  |------- "Kamal\n" ------------->|  (envoi du pseudo)
  |                                |  (vérification unicité)
  |<-- "*** Kamal joined ***" -----|  (broadcast aux autres)
  |         [session active]       |
  |------- "Bonjour\n" ----------->|
  |                                |  (broadcast "[Kamal]: Bonjour")
  |------- "/quit\n" ------------->|
  |<-- TCP close() ----------------|
```

## 4. Choix techniques et difficultés

### Langage : C

- Contrôle direct des sockets et de la mémoire
- Bibliothèque standard `<pthread.h>` pour la concurrence
- Performances adaptées à un serveur de chat

### Concurrence : un thread par client

Le serveur crée un nouveau thread POSIX (`pthread_create`) pour chaque client accepté. Chaque thread gère indépendamment la réception des messages de son client, ce qui garantit que **les clients lents ne bloquent pas les autres**.

Les threads sont détachés (`pthread_detach`) pour libérer automatiquement leurs ressources à la fin.

### Gestion de la liste des clients

Les clients connectés sont stockés dans une **liste chaînée** (`ClientNode*`). L'accès concurrent est protégé par un **mutex POSIX** (`pthread_mutex_t`) pour éviter les conditions de course lors des ajouts, suppressions et broadcasts.

### Affichage côté client

Un thread secondaire écoute les messages entrants en continu. Lorsqu'un message arrive, le client utilise les **codes d'échappement ANSI** (`\r\033[K`) pour effacer la ligne de saisie courante avant d'afficher le message, puis réaffiche le prompt `You:`. Cela évite l'entrelacement visuel entre ce que tape l'utilisateur et les messages reçus.

### Difficultés rencontrées

| Difficulté | Solution apportée |
|---|---|
| Race condition sur la liste des clients | Mutex autour de toutes les opérations sur `head` |
| Affichage entrelacé (saisie vs messages reçus) | Codes ANSI `\r\033[K` pour effacer la ligne |
| Redémarrage rapide du serveur | Option socket `SO_REUSEADDR` |
| Fuite mémoire si thread non détaché | `pthread_detach(pthread_self())` en début de thread |
| Déconnexion brutale (crash client) | `recv()` renvoie 0 ou -1 → nettoyage automatique |

## 5. Commandes disponibles

| Commande | Description |
|---|---|
| `/quit` | Se déconnecter proprement du serveur |

> **Note :** Le texte saisi sans commande est diffusé à tous les participants (mode salon général).
