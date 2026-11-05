# ChaqRec v0.3.0

Visualizador de audio flotante y grabador de micrófono para Windows 10/11 x64, hecho con **C++17 y Win32**. La interfaz y grabación siguen siendo ligeras. La **transcripción es opcional** y se ejecuta únicamente *después* de detener una grabación, en un proceso externo.

> **Estado de esta entrega:** código fuente completo para GitHub Actions. No se ha compilado ni probado el ejecutable en Windows durante la preparación de este ZIP. La compilación y las pruebas de grabación y transcripción son obligatorias antes de publicar una Release estable.

## Qué incluye

- Espectro fluido, onda suave y barras redondeadas con antialiasing y transparencia alfa.
- Colores cian, violeta, blanco, verde, azul oscuro y azul eléctrico; sensibilidad 1–10, suavizado, filtro de ruido y opacidad 30–100 %.
- **Audio del visualizador → Sonido de Windows / Micrófono / Ambos**. «Ambos» analiza los dos flujos independientemente para que el espectro responda también cuando solo está activo el micrófono. **La selección afecta al visualizador, no a la fuente de la grabación.**
- Overlay de escritorio con opción «Siempre visible»: se recupera automáticamente de una minimización accidental mediante Win+D, mientras permanezca habilitado y no se haya pulsado «Ocultar». En escritorios seguros (UAC), pantalla de bloqueo o aplicaciones exclusivas a pantalla completa Windows puede impedir mostrarlo.
- Grabación del **micrófono** en M4A/AAC de 96 kbps o WAV sin compresión, con indicador rojo minimalista (sin texto REC).
- Tres acciones al detener la grabación:
  - **Solo grabar audio:** guarda M4A o WAV; no descarga nada.
  - **Solo transcribir (TXT):** graba el micrófono temporalmente y, después de una transcripción exitosa, conserva el `.txt` y elimina el audio original. En caso de error conserva el audio.
  - **Grabar y transcribir (audio + TXT):** conserva ambos archivos.
- «Acerca de» con enlaces a [GitHub](https://github.com/Filter27/ChaqRec) y [apoyar el proyecto en PayPal](https://www.paypal.com/paypalme/Filter27).
- Interfaz español/inglés, icono de bandeja, preferencias persistentes e inicio automático opcional.

## Transcripción local gratuita

Utiliza [whisper.cpp](https://github.com/ggml-org/whisper.cpp) (licencia MIT), que GitHub Actions **compila y empaqueta como** `tools/whisper-cli.exe`. Se conserva separado de `ChaqRec.exe` para que la grabadora no cargue un modelo de IA al iniciar ni mientras captura audio.

Al seleccionar «Solo transcribir» o «Grabar y transcribir», ChaqRec pide confirmación e informa que el primer uso descargará el modelo multilingüe **`ggml-base.bin` (~148 MB)** desde [Hugging Face](https://huggingface.co/ggerganov/whisper.cpp). El modelo se almacena en `%LOCALAPPDATA%\ChaqRec\Models\ggml-base.bin` y se comprueba contra su tamaño y SHA-256 antes de usarse. Se descarga **solo cuando finaliza la primera grabación que necesita transcripción**. Las siguientes transcripciones se procesan sin internet.

Para cada grabación se usa Windows Media Foundation para crear un WAV temporal de 16 kHz mono y se ejecuta Whisper en otro proceso de CPU con un máximo de cuatro hilos. Se elimina el WAV temporal al terminar. Los TXT se guardan junto al audio en `Documentos\ChaqRec\Recordings`.

**Recursos:** Whisper base usa normalmente varios cientos de MB de RAM durante el procesamiento, pero ese consumo ocurre en `whisper-cli.exe` y se libera al finalizar. Grabar/visualizar no necesita el modelo. La transcripción puede ser más lenta que la duración del audio en computadores modestos y su precisión depende de ruido, voces y acentos. Los TXT no incluyen identificación fiable de participantes.

**Privacidad:** después de la descarga inicial, audio y transcripción permanecen en tu PC; ChaqRec no sube grabaciones a servidores. La única descarga de red introducida es el modelo desde Hugging Face, bajo consentimiento. No transcribas conversaciones sin la autorización aplicable.

**Limitación importante:** la **grabación** sigue capturando únicamente el micrófono predeterminado. Seleccionar «Ambos» en el **visualizador** no mezcla el audio de Windows en los M4A/WAV. Para transcribir reuniones virtuales completas, las voces remotas deben estar presentes en la entrada del micrófono u otra solución de captura de audio; esta versión no garantiza capturar las voces de Teams o Zoom de forma interna.

## Cómo crear el EXE desde GitHub

1. Extrae `ChaqRec-v0.3.0-source.zip` y abre la carpeta `ChaqRec`.
2. Abre [Filter27/ChaqRec](https://github.com/Filter27/ChaqRec) o crea un repositorio nuevo.
3. Sube **el contenido interior** a la raíz del repositorio (no el ZIP). Comprueba que existan `src/`, `assets/`, `CMakeLists.txt`, y **`.github/workflows/build.yml`**.
4. Ve a **Actions → Windows portable build**, selecciona `main` y ejecuta **Run workflow** si no se inició automáticamente.
5. Si la ejecución queda verde, descarga el artefacto `ChaqRec-Windows-x64-portable`. Dentro encontrarás `ChaqRec-v0.3.0-Windows-x64-portable.zip`.
6. Extrae el ZIP portable. Debe tener **`ChaqRec.exe` y `tools/whisper-cli.exe`**. Mantén ambos en sus rutas; solo se descarga el modelo cuando lo autorizas.
7. Prueba manualmente la grabación, ambas fuentes del visualizador, Win+D, la descarga del modelo y los TXT antes de publicar.

Si la pestaña Actions muestra «Get started with GitHub Actions», **no elijas plantillas de CMake**: significa que falta el archivo oculto `.github/workflows/build.yml`. Puedes crearlo desde **Code → Add file → Create new file** con esa ruta y pegar el YAML que se incluye en este ZIP. Instrucciones detalladas en [INSTRUCCIONES-GITHUB.md](INSTRUCCIONES-GITHUB.md).

## Compilar ChaqRec.exe localmente (opcional)

En Windows con Visual Studio 2022 y CMake:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

El EXE nativo resulta en `build/bin/Release/ChaqRec.exe`. Para **transcribir** también necesitas compilar por separado `whisper-cli.exe` de la versión **v1.9.5** de whisper.cpp y ubicarlo en `tools/whisper-cli.exe` respecto del EXE. GitHub Actions hace eso automáticamente.

## Estructura

| Archivo | Función |
|---|---|
| `src/main.cpp` | Menús, enlaces, mostrar escritorio, estado de transcripción |
| `src/Audio.cpp`, `Audio.h` | WASAPI, FFT, monitorización dual y grabación micrófono |
| `src/Transcriber.cpp`, `Transcriber.h` | Descarga verificada, conversión M4A/WAV y proceso Whisper |
| `src/M4aRecorder.cpp` | AAC/M4A con Windows Media Foundation |
| `src/Renderer.cpp` | Overlay GDI+ con transparencia ARGB |
| `src/Settings.cpp`, `SettingsDialog.cpp` | INI, ajustes y controles |
| `.github/workflows/build.yml` | Compila ChaqRec y whisper-cli y empaqueta ambos |
| `INSTRUCCIONES-GITHUB.md` | Guía desde cero / actualización del repositorio |
| `docs/TESTING.md` | Pruebas antes de publicar |

**Licencia del proyecto:** MIT. Whisper.cpp y sus dependencias mantienen sus licencias originales. Este proyecto no está afiliado con PayPal, Microsoft, Hugging Face ni con los desarrolladores de whisper.cpp.
