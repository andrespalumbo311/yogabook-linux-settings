# Progetto Futuro: Kernel Patch PMIC Wakeup su Connessione Caricatore (PR jekhor / Upstream)

- **Target Repository**: [`jekhor/yogabook-linux-kernel`](https://github.com/jekhor/yogabook-linux-kernel) & Linux Kernel Upstream (`linux-pm` / Hans de Goede)
- **Sottosistema**: Power Management / Extcon / PMIC (`extcon-intel-cht-wc`, `intel_soc_pmic_chtwc`, `bq25890_charger`)
- **Data Documento**: 15 Settembre 2026
- **Stato**: In pianificazione / Roadmap futura

---

## 1. Obiettivo del Progetto

Implementare e sottoporre una patch al kernel Linux (tramite Pull Request all'albero di **jekhor** e successivamente a **Hans de Goede** per il kernel mainline) per consentire al **Lenovo Yoga Book 1st Gen (YB1-X91F / YB1-X90F)** di risvegliarsi autonomamente dallo stato di Connected Standby (`s2idle`) non appena viene inserito il connettore micro-USB di ricarica.

---

## 2. Architettura Hardware e Contesto

Il Lenovo Yoga Book adotta una catena di alimentazione sofisticata ma non standard:
1. **PMIC**: Intel Whiskey Cove (`INT34D3:00`), gestito dai moduli `intel_soc_pmic_chtwc` e `extcon-intel-cht-wc` (`cht_wcove_pwrsrc`). Rileva la presenza fisica della linea VBUS e negozia il tipo di porta (SDP, DCP, CDP).
2. **Charger IC**: Texas Instruments BQ25892 (gestito da `bq25890_charger.ko`), collegato al bus I2C virtuale del PMIC (`i2c-14`).
3. **Batteria**: Texas Instruments BQ27542 (fuel gauge Li-ion da 8500 mAh / ~32 Wh su `i2c-0`).
4. **Alimentatore Rapido**: Adattatore proprietario Lenovo da 24W basato su standard **MediaTek Pump Express+ (PE+)** a tensione dinamica (5V -> 7V -> 9V -> 12V a 2A).

---

## 3. Descrizione del Problema e Perché la Patch è Necessaria

### Il limite del software-negotiated Pump Express
- Il chip TI BQ25892 non possiede un motore hardware autonomo per la negoziazione Pump Express+.
- La rampa di tensione da 5V a 12V è interamente guidata dal kernel Linux tramite la workqueue ritardata `bq25890_pump_express_work`, la quale invia sequenze di impulsi di corrente (`PUMPX_UP`) al caricatore per farlo commutare a 12V (riportato come ~11.3V nei log).
- Questa negoziazione richiede circa **15–25 secondi di tempo CPU attivo** (5 secondi di delay iniziale `PUMP_EXPRESS_START_DELAY` più i gradini di incremento tensione).

### Il blocco in Stand-by (`s2idle`)
- Quando il tablet è addormentato in Connected Standby (`s2idle`) o a coperchio chiuso:
  - Ispezionando il sottosistema IRQ (`/sys/kernel/irq/`):
    - `irq/119/wakeup` (`cht_wcove_pwrsrc`): **`disabled`**
    - `irq/134/wakeup` (`bq25890_irq`): **`disabled`**
  - Né `extcon-intel-cht-wc.c` né `intel_soc_pmic_chtwc.c` implementano chiamate a `enable_irq_wake()` nei rispettivi driver.
  - Durante l'ingresso in sonno, il kernel (`suspend_device_irqs()`) **maschera via hardware tutti gli interrupt non abilitati al wake**. Gli unici interrupt capaci di risvegliare la CPU sono il pulsante Power (`gpio-keys.3.auto`) e il sensore del coperchio (`PNP0C0D:00`).
- **Conseguenza drammatica**:
  1. Se l'utente collega il caricatore mentre il tablet è già in stand-by a coperchio chiuso, il PMIC rileva la tensione ma l'interrupt verso la CPU è mascherato.
  2. **Il SoC non si sveglia**. La workqueue `bq25890_pump_express_work` rimane congelata.
  3. L'alimentatore rimane bloccato alla tensione minima USB di **5V**.
  4. A 5V senza negoziazione attiva, il chip BQ25892 limita la corrente a **500 mA (SDP)**, erogando appena **~2.1 Watt**.
  5. Per ricaricare la batteria da 32Wh servono **oltre 15 ore**, dando l'impressione all'utente che la ricarica "non sia partita" o sia "lentissima".
  6. Non appena il tablet viene risvegliato manualmente (aprendo il coperchio o premendo Power), la CPU si attiva, negozia i 12V in ~20 secondi e la potenza schizza immediatamente a **24W (10.8W netti in batteria)**, ricaricando al ritmo del ~40% all'ora.

---

## 4. Stato Attuale: Il Workaround in Userspace

Nel repository abbiamo implementato un mitigatore software:
- File: `bin/yogabook-charger-negotiate` (servizio `yogabook-charge-negotiate.service` con regola udev `65-yogabook-charging.rules`).
- `systemd-logind.conf.d/yogabook.conf` con `LidSwitchIgnoreInhibited=no`.
- **Come funziona**: Quando il tablet è acceso (o viene risvegliato) e rileva il cavo, acquisisce un blocco inibitore `handle-lid-switch:sleep` per 25 secondi, impedendo a logind di riaddormentarlo finché il kernel non ha agganciato i 12V (>1.8A in batteria).
- **Limite del workaround**: Non può risolvere l'inserimento a tablet già addormentato. L'utente deve necessariamente premere il tasto Power per innescare il ciclo.

---

## 5. Dettagli Tecnici della Patch Kernel da Sviluppare

Per risolvere definitivamente alla radice il problema senza richiedere alcun intervento manuale dell'utente, è necessaria una patch a livello di driver kernel:

### A. Driver PMIC Extcon (`drivers/extcon/extcon-intel-cht-wc.c`)
1. **Wakeup Capability nel Probe**:
   ```c
   device_init_wakeup(ext->dev, true);
   ```
2. **Gestione Power Management (`dev_pm_ops`)**:
   Implementare i callback `suspend` e `resume`:
   ```c
   static int cht_wc_extcon_suspend(struct device *dev)
   {
       struct cht_wc_extcon_data *ext = dev_get_drvdata(dev);
       if (device_may_wakeup(dev))
           enable_irq_wake(ext->irq);
       return 0;
   }

   static int cht_wc_extcon_resume(struct device *dev)
   {
       struct cht_wc_extcon_data *ext = dev_get_drvdata(dev);
       if (device_may_wakeup(dev))
           disable_irq_wake(ext->irq);
       return 0;
   }
   ```
3. Registrare `SIMPLE_DEV_PM_OPS(cht_wc_extcon_pm_ops, cht_wc_extcon_suspend, cht_wc_extcon_resume)` nel `platform_driver`.

### B. MFD Parent Driver (`drivers/mfd/intel_soc_pmic_chtwc.c`)
- Assicurarsi che l'IRQ genitore del chip interrupt Whiskey Cove (`irq_chip`) e l'ACPI Event GPIO (`chv-gpio 19` / IRQ 116) inoltrino correttamente la richiesta di `set_wake` dal dominio virtuale al dominio GPIO/SoC hardware (`irq_set_irq_wake`).

---

## 6. Procedura di Realizzazione e Invio

1. **Setup Ambiente di Build**:
   - Clonare il repository sorgente di `jekhor/yogabook-linux-kernel`.
   - Isolare il branch o il commit corrispondente alla release installata (`6.17.4-1-yogabook`).
2. **Sviluppo & Test Locale**:
   - Applicare le modifiche su `drivers/extcon/extcon-intel-cht-wc.c`.
   - Compilare il modulo/kernel con `makepkg -s` su Arch Linux.
   - Installare il pacchetto test e verificare che `/sys/kernel/irq/119/wakeup` passi a `enabled`.
   - Verificare in `s2idle`: collegare il cavo micro-USB a tablet addormentato e verificare che il sistema esca istantaneamente da `s2idle` loggando il resume nei log di sistema.
3. **Pull Request**:
   - Inviare la PR con descrizione tecnica dettagliata a [jekhor/yogabook-linux-kernel](https://github.com/jekhor/yogabook-linux-kernel).
4. **Upstream Submission**:
   - Inviare patch formattata con `git format-patch` a:
     - Hans de Goede <hdegoede@redhat.com>
     - linux-pm@vger.kernel.org
     - linux-kernel@vger.kernel.org
