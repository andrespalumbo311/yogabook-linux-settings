# Lenovo Yoga Book (YB1-X91F) – Report Completo delle Configurazioni e Ottimizzazioni

Questo documento riassume tutte le modifiche, correzioni hardware/software e ottimizzazioni di sistema implementate su **Arch Linux (kernel 6.17.4-1-yogabook)** con **MangoWC** e **DMS (Dank Material Shell)**.

---

## 1. Ottimizzazione Grafica e Compositor (MangoWC)
* **File di configurazione**: [`/home/andres/.config/mango/config.conf`](file:///home/andres/.config/mango/config.conf)
* **Alleggerimento rendering per CPU Intel Atom e GPU Gen8**:
  - **Blur disattivato**: `blur=0`, `blur_layer=0` (eliminati gli shader multi-pass di SceneFX).
  - **Ombre disattivate**: `shadows=0`, `layer_shadows=0` (nessun clipping o sfocatura offscreen).
  - **Animazioni disattivate**: `animations=0`, `layer_animations=0` (eliminato il loop continuo a 60fps per il calcolo delle transizioni).
  - **Opacità fissa**: `focused_opacity=1.0`, `unfocused_opacity=1.0` (azzerati i passaggi di alpha blending nel compositor).
  - **Drag & snap disabilitati**: `drag_tile_to_tile=0`, `enable_floating_snap=0`.
  - **Sincronizzazione esplicita**: `syncobj_enable=1`, `warpcursor=1`, `sloppyfocus=1`.
* **Personalizzazioni grafiche e scorciatoie**:
  - **Bordi arrotondati**: `border_radius=10`, `borderpx=2`.
  - **Tasto Super + Invio**: apre il terminale rapido `foot`.
  - **Tasto Super + Spazio**: apre il launcher applicazioni di DMS (`dms ipc call launcher toggle`).
  - **Tasto Super + 1..9**: cambio rapido workspace (`dispatch workspace 1..9`).
  - **Tasto Super + Shift + 1..9**: spostamento finestra su workspace (`dispatch movetoworkspace 1..9`).
  - **Touchpad**: invertito lo scorrimento a due dita (`trackpad_natural_scrolling=1` e `devicerule=name:virtual-touchpad,natural_scrolling:1`).
  - **Font**: installati font `ttf-jetbrains-mono-nerd` e `ttf-nerd-fonts-symbols`.
  - **Pulizia DE**: rimossi ambienti non utilizzati in precedenza (Hyprland, Ly).

---

## 2. Risoluzione Problemi Package Manager (Pamac / Pacman 7)
* **Problema riscontrato**: `libpamac` falliva durante il download dei database dei pacchetti con errore Landlock (`Error: restricting filesystem access failed because Landlock is not supported by the kernel!`) e fallimento nel passaggio all'utente sandbox `alpm`.
* **Causa**: Pacman 7.0 attiva di default il sandboxing del filesystem tramite Landlock, non abilitato in questa specifica build del kernel Yoga Book.
* **Soluzione**:
  - Compilata la libreria intercettore [`/usr/lib/pamac_fix.so`](file:///usr/lib/pamac_fix.so) che fa ritornare successo (`0`) a `alpm_sandbox_setup_child`.
  - Registrata in [`/etc/ld.so.preload`](file:///etc/ld.so.preload).
* **Risultato**: Pamac sincronizza i repository Arch e AUR istantaneamente e senza errori. È disponibile sia via terminale che tramite GUI ("Aggiungi/Rimuovi software") nel launcher DMS.

---

## 3. Rotazione Automatica Intelligente (Laptop vs Tablet/Flat/Book)
* **File eseguibile**: [`/home/andres/.local/bin/yogabook-autorotate`](file:///home/andres/.local/bin/yogabook-autorotate)
* **Unità systemd**: [`/home/andres/.config/systemd/user/rot8.service`](file:///home/andres/.config/systemd/user/rot8.service)
* **Verifica Empirica dei Sensori IIO**:
  - Testando l'inclinazione separata delle scocche, è emerso che nel database udev upstream le etichette erano invertite:
    - `iio:device2` (`ACCEL_LOCATION=base` in udev) è **fisicamente nello SCHERMO** (si muoveva inclinando solo il display).
    - `iio:device4` (senza location in udev) è **fisicamente nella TASTIERA** (rimaneva fermo a $Z \approx -1g$ sul tavolo).
  - Lo script associa dinamicamente:
    - Schermo $\rightarrow$ sensore con `ACCEL_LOCATION=base`
    - Base tastiera $\rightarrow$ sensore non-base
* **Proiezione 2D nel Piano Perpendicolare alla Cerniera**:
  - Allineamento assi base-schermo: $\vec{b}_{\text{al}} = (b_y, -b_x, b_z)$.
  - L'asse fisico della cerniera corrisponde all'asse $Y$ dello schermo.
  - La rotazione della cerniera avviene esclusivamente nel piano $(X, Z)$:
    $$\vec{v}_s = (s_x, s_z), \quad \vec{v}_b = (b_{\text{al}}[0], b_{\text{al}}[2]) = (b_y, b_z)$$
    $$\text{cross}_{2D} = s_x \cdot b_z - s_z \cdot b_y, \quad \text{dot}_{2D} = s_x \cdot b_y + s_z \cdot b_z$$
    $$\text{apertura} = (180.0^\circ - \text{atan2}(\text{cross}_{2D}, \text{dot}_{2D})) \pmod{360^\circ}$$
  - **Immunità al Rollio laterale**: la proiezione 2D cancella al 100% l'effetto di rollio/inclinazione laterale fino a oltre $70^\circ$, misurando l'angolo reale della cerniera in qualunque postura.
* **Guardia di Singolarità della Cerniera**:
  - Quando il dispositivo è ruotato di $90^\circ$ sul fianco (cerniera verticale), la gravità agisce lungo l'asse $Y$. I vettori perpendicolari $\vec{v}_s$ e $\vec{v}_b$ tendono a zero.
  - In questa condizione singolare, la gravità non può fisicamente misurare l'angolo della cerniera.
  - **Soluzione applicata**: la guardia (`perp_s < 0.35` e `perp_b < 0.35`) **congela la modalità attiva** (`laptop` o `tablet`), impedendo categoricamente di uscire dalla modalità laptop quando il dispositivo viene sollevato o inclinato lateralmente.
* **Isteresi e Modalità di Funzionamento**:
  - **Modalità LAPTOP**: base appoggiata in piano ($b_z < -0.35g$) e apertura $\le 150.0^\circ$. Rotazione automatica **disattivata**, display forzato a **`270`** (landscape standard), tastiera virtuale chiusa.
  - **Modalità TABLET / FLAT / BOOK**: apertura $\ge 158.0^\circ$ (o scocca ribaltata a $360^\circ$). Rotazione automatica **attiva**.
  - **Debounce di transizione**: 2 cicli consecutivi (~0.8s) prima di commutare modalità per evitare falsi positivi durante il movimento.
* **Rotazione Wayland Diretta con Flat-Lock e Isteresi Decisa**:
  - Mappatura assi verificata:
    - Schermo $s_y > +0.35$ $\rightarrow$ `180` (Verticale standard / Modalità libro)
    - Schermo $s_y < -0.35$ $\rightarrow$ `normal` (Verticale invertito)
    - Schermo $s_x < -0.35$ $\rightarrow$ `270` (Orizzontale standard)
    - Schermo $s_x > +0.35$ $\rightarrow$ `90` (Orizzontale invertito / Tenda)
  - **Flat-Lock a 53° ($|z| > 0.60$)**: quando lo schermo è inclinato a meno di 53° rispetto al piano orizzontale (es. appoggiato o calato sulla scrivania), l'orientamento viene **congelato**. Questo impedisce il ritorno accidentale in landscape quando il dispositivo viene appoggiato mentre era in portrait.
  - **Isteresi di Cambio Deciso (Rapporto 1.35x e forza minima 0.50g)**: passaggio fluido e stabile solo su decisa riorientazione intenzionale.
  - **Filtro Debounce Orientamento (2 cicli / ~0.8s)**: elimina ogni scatto o rotazione transitoria della mano.

---

## 4. Tastiera On-Screen (Virtual Keyboard)
* **Binario installato**: [`/home/andres/.local/bin/wvkbd`](file:///home/andres/.local/bin/wvkbd) (`wvkbd-mobintl`), tastiera nativa Wayland/wlroots.
  - Consumo di memoria: **solo ~1.4 MB di RAM**.
  - Livelli tastiera: completa (full QWERTY), caratteri speciali, emoji.
  - Stile grafico: bordi arrotondati (`-R 10`) e palette colori Catppuccin coordinata con DMS.
* **Servizio di background**: [`/home/andres/.config/systemd/user/wvkbd.service`](file:///home/andres/.config/systemd/user/wvkbd.service) (avviata nascosta con flag `--hidden`, latenza di apparizione pari a 0 ms).
* **Script di toggle**: [`/home/andres/.local/bin/toggle-keyboard`](file:///home/andres/.local/bin/toggle-keyboard) (invia `SIGRTMIN` per mostrare/nascondere istantaneamente).
* **Widget DMS nella barra superiore**:
  - Plugin DMS creato in [`/home/andres/.config/DankMaterialShell/plugins/VirtualKeyboard/`](file:///home/andres/.config/DankMaterialShell/plugins/VirtualKeyboard/).
  - Icona tastiera ⌨️ integrata nella sezione destra della barra superiore di DMS.

---

## 5. Widget "Chiudi Finestra" su Barra DMS
* **Plugin DMS**: [`/home/andres/.config/DankMaterialShell/plugins/CloseWindow/`](file:///home/andres/.config/DankMaterialShell/plugins/CloseWindow/)
* Pulsante con icona ✕ rossa nella barra superiore di DMS che invia il comando IPC `mmsg dispatch killclient` per chiudere all'istante la finestra attiva.

---

## 6. Prestazioni, Ottimizzazioni Kernel e Alleggerimento Storage
* **Recuperati ~7.5 GB di spazio su disco** (spazio occupato sceso da 16 GB a 8.5 GB su eMMC da 64 GB):
  - Eliminata la cache di build AUR in `~/.cache/paru/clone` (**-2.5 GB**).
  - Prunata la cache dei vecchi pacchetti scaricati in `/var/cache/pacman/pkg` (**-3.7 GB**).
  - Rimossi pacchetti orfani di compilazione pesanti non più necessari (`rust`, `clang`, `lld`, `meson`, `gn`, `rust-bindgen`, `scenefx0.4`, ecc.) (**-653 MB**).
* **Ottimizzazione I/O eMMC Flash ([`/etc/fstab`](file:///etc/fstab))**:
  - Root montata con `rw,noatime,commit=60`.
  - `noatime`: elimina le continue scritture sulla memoria flash ad ogni lettura di file, icone e librerie.
  - `commit=60`: i metadati ext4 vengono scaricati ogni 60 secondi invece di 5, prolungando il riposo a basso consumo della flash eMMC.
* **Prevenzione blocchi/lag dell'interfaccia ([`/etc/sysctl.d/99-zram-performance.conf`](file:///etc/sysctl.d/99-zram-performance.conf))**:
  - Aggiunti `vm.dirty_ratio = 10` e `vm.dirty_background_ratio = 5`. Su memorie eMMC lente, evita i freeze causati dallo scaricamento improvviso di grossi blocchi di dati sporchi dalla RAM.
  - ZRAM configurata su 4 GB (`zram0`) con algoritmo veloce `lzo-rle`, `vm.swappiness = 180` e `vm.page-cluster = 0` (zero amplificazione di lettura swap-in).
* **Limite log di sistema Journald**:
  - Configurato [`/etc/systemd/journald.conf.d/00-size-limit.conf`](file:///etc/systemd/journald.conf.d/00-size-limit.conf) con limite a **50 MB** (`SystemMaxUse=50M`, `RuntimeMaxUse=30M`), evitando crescite fino a 4 GB.
* **Gestione Servizi Background**:
  - Disattivato `accounts-daemon.service` (inutile per DMS/Mango).
  - **`sshd.service` mantenuto attivo** come richiesto per l'accesso remoto.

---

## 7. Accelerazione Hardware Video & Monitoraggio
* **Driver VA-API**: verificato funzionamento del driver `i965` (Intel Cherryview Gen8) con decodifica hardware per H.264, VP8 e HEVC 8-bit.
* **YouTube / Browser**: impostata estensione **enhanced-h264ify** per forzare lo stream su H.264 (decodificato in hardware dalla GPU Gen8) ed escludere VP9/AV1 (che saturerebbero la CPU Atom al 100%).
* **Strumenti installati**:
  - `nvtop`: monitor delle risorse GPU e CPU da terminale.
  - `pamac-manager`: app store grafico.

---

## 8. Gestione Standby / Sospensione (S2idle, PWM Backlight & Logind)
* **Diagnosi del Problema Riscontrato**:
  - Alla chiusura del coperchio, il sistema andava correttamente in sospensione (`s2idle` / Connected Standby).
  - Alla riapertura, il sistema si risvegliava (`PM: suspend exit`), ma **lo schermo rimaneva completamente nero**.
  - **Causa primaria**: Nel log di avvio del kernel era presente l'errore:
    `i915 0000:00:02.0: [drm] *ERROR* [CONNECTOR:115:DSI-1] Failed to get the SoC PWM chip`
    Il driver video `i915` non trovava il controller PWM hardware del SoC Intel Atom Cherry Trail all'avvio perché i moduli `pwm_lpss` e `pwm_lpss_platform` non erano presenti nell'initramfs. Di conseguenza, il controller nativo `intel_backlight` non veniva registrato e la retroilluminazione del pannello DSI non si riaccendeva al risveglio dal suspend.
  - **Causa secondaria (spegnimento accidentale)**: Con lo schermo nero, premendo brevemente il tasto di accensione per riattivare lo schermo, `systemd-logind` interpretava la pressione con l'azione di default `HandlePowerKey=poweroff`, spegnendo istantaneamente il PC.
* **Correzioni Applicate**:
  1. **Initramfs aggiornato ([`/etc/mkinitcpio.conf`](file:///etc/mkinitcpio.conf))**:
     - Aggiunti i moduli precoci: `MODULES=(pwm_lpss pwm_lpss_platform i915)`.
     - Rigenerata l'immagine con `mkinitcpio -p linux-yogabook`. In questo modo il chip PWM è disponibile prima che `i915` inizializzi il display DSI, permettendo la corretta gestione e riaccensione della retroilluminazione dopo la sospensione.
  2. **Configurazione Logind ([`/etc/systemd/logind.conf.d/yogabook.conf`](file:///etc/systemd/logind.conf.d/yogabook.conf))**:
     - `HandleLidSwitch=suspend` (sospensione alla chiusura del coperchio).
     - `HandlePowerKey=suspend` (la pressione breve del tasto di accensione sospende o risveglia il PC anziché spegnerlo).
     - `HandlePowerKeyLongPress=poweroff` (lo spegnimento completo avviene solo tenendo premuto a lungo il tasto).
