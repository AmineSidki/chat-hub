# Documentation Technique — Chat Hub

## Schéma d'architecture

```
                       ┌──────────────────────────────────────────────┐
                       │              SERVEUR  Chat Hub                │
                       │                                               │
    ┌──────────┐  TCP  │  ┌─────────────┐     ┌────────────────────┐  │
    │ Client 1 │◄─────►│  │ accept()    │────►│ handle_client()    │  │
    │ (Kamal)  │       │  │ thread main │     │ 1 pthread/client   │  │
    └──────────┘       │  └─────────────┘     └────────┬───────────┘  │
                       │                               │               │
    ┌──────────┐  TCP  │                               ▼               │
    │ Client 2 │◄─────►│              ┌────────────────────────────┐   │
    │ (Said)   │       │              │   broadcast_message()      │   │
    └──────────┘       │              │   diffuse à tous sauf      │   │
                       │              │   le client émetteur       │   │
    ┌──────────┐  TCP  │              └────────────┬───────────────┘   │
    │ Client N │◄─────►│                           │                   │
    └──────────┘       │              ┌────────────▼───────────────┐   │
                       │              │  Liste chaînée  ClientNode  │  │
                       │              │  { socket_fd, username,    │   │
                       │              │    ip_address, *next }     │   │
                       │              │  ← protégée par mutex →    │   │
                       │              └────────────────────────────┘   │
                       └──────────────────────────────────────────────┘
```

### Composants côté client

Chaque processus client démarre **deux threads** :

| Thread | Rôle |
|---|---|
| Thread principal (`main`) | Lit la saisie clavier et envoie les messages au serveur via `send()` |
| Thread secondaire (`listen_handler`) | Bloque sur `recv()` et affiche les messages entrants en temps réel |

---

## Description des messages échangés

### Transport

- **Protocole** : TCP/IP — connexion persistante, fiable, ordonnée (FIFO garanti).
- **Encodage** : texte brut UTF-8, terminé par `\n`.
- **Port** : configurable au lancement (ex. `5555`).

---

### 1. Séquence de connexion

```
Client                              Serveur
  │                                    │
  │──── TCP connect() ────────────────►│
  │                                    │  ← accept() débloqué
  │                                    │  ← pthread_create(handle_client)
  │                                    │
  │──── "Kamal\n"  (pseudo) ──────────►│
  │                                    │  ← vérification unicité (mutex)
  │                                    │
  │                                    │── broadcast → autres clients :
  │                                    │   "\n*** Kamal joined the chat ***\n"
  │                                    │
  │         [session active]           │
```

Si le pseudo est déjà pris :

```
  │◄─── "ERROR: Username already taken. Disconnecting...\n"
  │──── TCP close()
```

---

### 2. Catalogue des messages

#### Messages envoyés par le client → serveur

| Type | Format sur le réseau | Exemple |
|---|---|---|
| Enregistrement du pseudo | `<username>\n` | `Kamal\n` |
| Message public (salon général) | `<texte>\n` | `Bonjour tout le monde !\n` |
| Message privé | `/msg <pseudo> <texte>\n` | `/msg Said Salut !\n` |
| Déconnexion propre | `/quit\n` | `/quit\n` |
| Liste des utilisateurs | `/users\n` | `/users\n` |

#### Messages émis par le serveur → clients

| Événement | Format envoyé aux destinataires |
|---|---|
| Arrivée d'un utilisateur | `\n*** <pseudo> joined the chat ***\n` |
| Départ d'un utilisateur | `\n*** <pseudo> left the chat ***\n` |
| Pseudo déjà pris | `ERROR: Username already taken. Disconnecting...\n` |
| Message public | `[<pseudo>]: <texte>\n` |
| Message privé reçu | `[PM from <pseudo>]: <texte>\n` |

---

### 3. Séquence d'un message public

```
Client 1 (Kamal)                  Serveur                  Client 2 (Said)
       │                              │                           │
       │── "Bonjour tout le monde\n" ►│                           │
       │                              │  handle_client() reçoit   │
       │                              │  → snprintf("[Kamal]: …") │
       │                              │  → broadcast_message()    │
       │                              │──► "[Kamal]: Bonjour…\n" ►│
       │                              │                           │
```

---

### 4. Séquence d'un message privé

```
Client 1 (Kamal)                  Serveur                  Client 2 (Said)
       │                              │                           │
       │── "/msg Said Salut !\n" ─────►│                           │
       │                              │  parse /msg               │
       │                              │  → trouve Said dans list  │
       │                              │──────────────────────────►│
       │                              │  "[PM from Kamal]: Salut !"│
       │                              │                           │
       │    (les autres clients ne reçoivent rien)                │
```

---

### 5. Séquence de déconnexion

```
Client (Said)                     Serveur                  Autres clients
      │                               │                          │
      │── "/quit\n" ─────────────────►│                          │
      │── TCP close() ───────────────►│                          │
      │                               │  recv() renvoie 0        │
      │                               │  → remove_client()       │
      │                               │  → broadcast :           │
      │                               │──────────────────────────►
      │                               │  "*** Said left ***\n"   │
```

> **Déconnexion brutale (crash client)** : `recv()` renvoie `-1` ou `0`, ce qui déclenche le même nettoyage automatique.

---

## Garanties du protocole

| Garantie | Mécanisme |
|---|---|
| Ordre FIFO des messages | TCP garantit l'ordre dans chaque connexion |
| Unicité des pseudos | Vérification sous mutex avant ajout à la liste |
| Pas d'interblocage | 1 thread par client — les clients lents n'affectent pas les autres |
| Libération des ressources | `pthread_detach()` en début de thread + `free()` sur déconnexion |
| Redémarrage rapide | Option socket `SO_REUSEADDR` sur le serveur |
| Affichage non entrelacé | Codes ANSI `\r\033[K` côté client |
