# Lenovo Yoga Book (YB1-X91F) – Linux Configurations & Dotfiles

Questo repository contiene tutte le configurazioni ufficiali, gli script personalizzati, i servizi di sistema e le ottimizzazioni hardware/software sviluppate per il **Lenovo Yoga Book (YB1-X91F)** su **Arch Linux** con **MangoWC** e **DMS (Dank Material Shell)**.

Tutti i file di configurazione attivi nell'ambiente utente (`~/.config/`, `~/.local/bin/`, plugin DMS) sono **collegati tramite collegamenti simbolici (symlink)** ai file di questo repository. Modificando i file all'interno di questa cartella, le modifiche si rifletteranno istantaneamente nel sistema.

---

## 📁 Struttura del Repository

```text
yogabook-config/
├── README.md                      # Questa documentazione
├── YOGABOOK_SETUP_REPORT.md       # Report dettagliato con spiegazione tecnica di tutti gli interventi
├── install.sh                     # Script per creare/ripristinare tutti i symlink
│
├── bin/                           # Script eseguibili utente (~/.local/bin)
│   ├── yogabook-autorotate        # Daemon intelligente di auto-rotazione (apertura cerniera, Flat-Lock, guardia singolarità)
│   ├── toggle-keyboard            # Toggle rapido visibilità tastiera a schermo wvkbd
│   └── wvkbd                      # Binario tastiera Wayland on-screen con bordi arrotondati
│
├── config/                        # Configurazioni utente (~/.config)
│   ├── mango/
│   │   └── config.conf            # Configurazione super-ottimizzata di MangoWC (basso consumo CPU/GPU, Wacom mapping)
│   ├── systemd/
│   │   └── user/
│   │       ├── rot8.service       # Servizio utente per yogabook-autorotate
│   │       └── wvkbd.service      # Servizio utente per wvkbd (avvio in background con flag --hidden)
│   └── DankMaterialShell/         # Personalizzazioni e plugin DMS
│       ├── plugin_settings.json   # Impostazioni attive dei plugin DMS
│       ├── plugins.lock.json      # Stato di attivazione dei plugin DMS
│       └── plugins/
│           ├── VirtualKeyboard/   # Widget tastiera a schermo nella barra superiore di DMS
│           └── CloseWindow/       # Widget chiudi finestra (pulsante ✕ rosso) nella barra superiore
│
└── system/                        # Copie e sorgenti dei file di configurazione di sistema (/etc)
    ├── pamac_fix/                 # Modulo C per bypassare il limite Landlock del kernel su Pacman 7 / libpamac
    │   ├── pamac_fix.c
    │   └── Makefile
    └── etc/
        ├── fstab                  # Ottimizzazioni I/O per memoria flash eMMC (noatime, commit=60)
        ├── mkinitcpio.conf        # Moduli precoci (pwm_lpss, pwm_lpss_platform, i915) per ripristino backlight dopo standby
        ├── ld.so.preload          # Preload per pamac_fix.so
        ├── sysctl.d/
        │   └── 99-zram-performance.conf # Parametri ZRAM (swappiness=180) e gestione dirty memory su flash lenta
        ├── systemd/
        │   ├── journald.conf.d/
        │   │   └── 00-size-limit.conf   # Limite dimensione log journald a 50 MB
        │   └── logind.conf.d/
        │       └── yogabook.conf        # Azione sospensione su chiusura coperchio e pressione tasto power
        └── touch_keyboard/
            ├── touch-hw.csv       # Parametri fisici della tastiera Halo / Create Pad (rotazione 270°)
            └── layout.csv         # Mappa tastiera attiva
```

---

## 🚀 Installazione e Ripristino dei Symlink

Per ripristinare o creare i symlink sui percorsi di configurazione dell'utente:

```bash
cd ~/yogabook-config
./install.sh
```

Per aggiornare anche i file di configurazione in `/etc` (richiede privilegi `sudo`):

```bash
./install.sh --system
```

---

## 🛠️ Riepilogo dei Componenti Chiave

1. **Auto-Rotazione Cerniera e Schermo (`bin/yogabook-autorotate`)**:
   - Calcola in tempo reale l'angolo di apertura reale ($0^\circ-360^\circ$) proiettando i vettori gravità nel piano perpendicolare alla cerniera.
   - **Laptop mode ($\le 150^\circ$)**: display bloccato fisso in landscape (`270`), rotazione disattivata, tastiera virtuale nascosta.
   - **Tablet / Flat mode ($\ge 158^\circ$)**: rotazione automatica attiva.
   - **Flat-Lock a 53°**: congela l'orientamento quando il dispositivo viene appoggiato in piano su un tavolo, impedendo rotazioni indesiderate.
   - **Guardia di Singolarità**: mantiene lo stato attivo durante il sollevamento o rotazione laterale del portatile.

2. **Digitalizzatore Wacom Create Pad (`config/mango/config.conf`)**:
   - Corretto l'orientamento con `tablet_map_to_mon=DSI-1`, allineando il digitalizzatore 1:1 con la rotazione hardware del display.

3. **Standby / Sospensione Profonda (`system/etc/mkinitcpio.conf` & `logind.conf.d`)**:
   - Risolto lo schermo nero al risveglio dal suspend inserendo i moduli `pwm_lpss`, `pwm_lpss_platform` e `i915` nell'initramfs, garantendo il corretto funzionamento del controller PWM della retroilluminazione.
   - Chiusura lid configurata su sospensione `s2idle`, tasto power configurato per sospendere / risvegliare invece di spegnere.

4. **Tastiera Virtuale a Basso Consumo (`bin/wvkbd`, `bin/toggle-keyboard`)**:
   - Consumo RAM ridotto a soli ~1.4 MB, latenza 0 ms, integrata con widget nella barra DMS.

5. **Fix Pacman 7 / Pamac GUI (`system/pamac_fix/`)**:
   - Bypass del sandboxing Landlock per consentire la sincronizzazione istantanea dei repository su kernel con configurazioni Landlock disabilitate.
