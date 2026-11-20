# Rimac en el DAW

## Escaneo del plugin

| DAW | Ruta / pasos |
|-----|--------------|
| **FL Studio** | Options → Manage plugins → *Find more plugins* → verifica que `C:\Program Files\Common Files\VST3` esté activo → *Start scan* |
| **Ableton Live** | Preferences → Plug-Ins → *Use VST3 Plug-In System Folders* activado → *Rescan* |
| **Logic / GarageBand** | El `.component` (AU) va en `~/Library/Audio/Plug-Ins/Components/`. Logic detecta al reiniciar. |
| **Reaper** | Preferences → Plug-ins → VST → *Re-scan*. Ruta VST3 por defecto ya incluida. |
| **Cubase / Nuendo** | Se detecta automáticamente al reiniciar (VST3). |
| **Studio One** | Preferences → Locations → VST3 → *Scan*. |
| **Bitwig** | Settings → Plug-ins → *Scan*. |

## Controles

| Control | Rango | Qué hace |
|---------|-------|----------|
| **Modo Soft / Hard** | — | Soft = transparente, umbral alto. Hard = agresivo, reacciona al nivel. |
| **Depth** | 0–100 % | Cantidad total de supresión. A 0 % el plugin es transparente. |
| **Attack** | 0.1–500 ms | Rapidez con la que empieza a atenuar una resonancia. |
| **Release** | 5–2000 ms | Rapidez con la que deja de atenuar. Largos = más musicales. |
| **Ecualización** | 20 Hz–20 kHz / ±24 dB | Enfoca la supresión en una banda. El switch la activa. |
| **Sidechain** | 20 Hz–20 kHz / −60–0 dB | Filtro del detector y umbral de sensibilidad. |
| **Procesamiento** | Mid / Side | Qué componente estéreo se procesa. |
| **Nodos del gráfico** | — | Arrastra para mover el foco de supresión (el nodo central mueve Freq y Gain). |

## Ajustes recomendados

- **Voz**: Soft, Depth 60 %, Attack 8 ms, Release 120 ms, EQ 1.2 kHz / −5 dB.
- **Batería / overheads**: Hard, Depth 75 %, Attack 3 ms, Release 80 ms, EQ 3 kHz / −4 dB.
- **Master**: Soft, Depth 35–45 %, Attack 15 ms, Release 250 ms, Side activado para controlar la resonancia del lado estéreo.
- **Piano / cuerdas**: Soft, Depth 50 %, Attack 20 ms, Release 300 ms.

## Latencia

El plugin reporta **1536 samples** (32 ms a 48 kHz, 16 ms a 96 kHz). Todos los DAWs modernos la compensan automáticamente; no necesitas ajustar nada a mano.
