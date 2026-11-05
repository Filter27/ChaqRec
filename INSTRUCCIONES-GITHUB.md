# ChaqRec v0.3.0 — Manual para publicar el código y crear `ChaqRec.exe` en GitHub

**Actualización:** noviembre de 2026 · **Idioma:** español · **Requisitos:** cuenta GitHub y navegador web.  
**Método:** sin Git, GitHub Desktop, Visual Studio ni comandos locales.  
**Proyecto:** `ChaqRec-v0.3.0-source.zip` o `ChaqRec-v0.3.0-source-con-manual.zip` · **Ejemplo de usuario:** `Filter27`.

> **Importante.** El ZIP de origen contiene **código fuente**, no el programa terminado. El ZIP portable generado contendrá ChaqRec.exe y tools/whisper-cli.exe; el modelo gratuito se descarga solo en la primera transcripción autorizada. GitHub Actions compila el `.exe` en un servidor Windows. El resultado necesita pruebas reales, especialmente para la grabación M4A. Este manual describe **dos opciones**: (A) crear un repositorio desde cero y (B) actualizar uno existente sin perder la versión que ya funciona.

## Índice

- [Antes de empezar](#antes-de-empezar)
- [Opción A. Crear un repositorio nuevo desde cero](#opción-a-crear-un-repositorio-nuevo-desde-cero)
- [Opción B. Actualizar un repositorio existente](#opción-b-actualizar-un-repositorio-existente)
- [Solución común: configurar GitHub Actions cuando no aparece el flujo](#solución-común-configurar-github-actions-cuando-no-aparece-el-flujo)
- [Compilar y descargar `ChaqRec.exe`](#compilar-y-descargar-chaqrecexe)
- [Probar la nueva versión](#probar-la-nueva-versión)
- [Publicar una versión para descargas](#publicar-una-versión-para-descargas)
- [Diagnóstico de errores frecuentes](#diagnóstico-de-errores-frecuentes)

---

## Antes de empezar

### 0.1. Extraer los archivos

1. Descarga el archivo **`ChaqRec-v0.3.0-source.zip`** o el ZIP actualizado **`ChaqRec-v0.3.0-source-con-manual.zip`**.
2. En Windows, haz clic derecho sobre él y elige **Extraer todo**.
3. Abre la carpeta `ChaqRec` que está dentro del ZIP. Si ves otra carpeta `ChaqRec` al abrirlo, entra en ella también.
4. Comprueba que ves los archivos y carpetas **dentro de `ChaqRec`**:

```text
ChaqRec/
├── .github/
│   └── workflows/
│       └── build.yml
├── assets/
├── docs/
├── src/
│   ├── main.cpp
│   ├── Audio.cpp
│   ├── Renderer.cpp
│   ├── SettingsDialog.cpp
│   ├── M4aRecorder.cpp
│   └── ...
├── CMakeLists.txt
├── README.md
├── LICENSE
├── PORTABLE-LEEME.txt
└── INSTRUCCIONES-GITHUB.md
```

**No subas únicamente el ZIP.** Se deben subir sus archivos y carpetas interiores, preservando la estructura. `CMakeLists.txt` debe quedar en la **raíz** del repositorio: `CMakeLists.txt`, no `ChaqRec/CMakeLists.txt`.

**Atención con `.github`:** es una carpeta que empieza por punto. En algunos exploradores de archivos o procedimientos de carga puede pasar inadvertida. Si termina faltando en GitHub, la crearemos manualmente siguiendo el apartado de solución común. No uses las plantillas sugeridas de la pantalla **Get started with GitHub Actions**.

### 0.2. Elegir cómo vas a trabajar

| Tu situación | Ruta recomendada |
|---|---|
| Quieres comenzar con un repositorio independiente | **Opción A**: crear `ChaqRec-v2` desde cero |
| Ya tienes `Filter27/ChaqRec` y quieres conservar su historia | **Opción B**: crear una rama y actualizar allí |
| Ya subiste todo, pero Actions muestra “Get started with GitHub Actions” | Ve directamente a **Solución común** |

---

## Opción A. Crear un repositorio nuevo desde cero

### A1. Crear el repositorio en GitHub

1. Abre <https://github.com/new> e inicia sesión.
2. En **Owner** elige tu cuenta, por ejemplo `Filter27`.
3. En **Repository name** escribe `ChaqRec-v2` (o `ChaqRec` **solo si no existe ya** en tu cuenta).
4. En **Description**, opcionalmente: `Visualizador de audio y grabador minimalista para Windows`.
5. Marca **Public** si quieres ofrecerlo para descarga pública.
6. **No** marques «Add a README file», «Add .gitignore» ni «Choose a license»: el proyecto ya los trae.
7. Pulsa **Create repository**.

Ejemplo de dirección del nuevo repositorio: <https://github.com/Filter27/ChaqRec-v2> (solo existirá después de que lo crees con ese nombre).

### A2. Subir el código fuente desde el navegador

1. Abre el nuevo repositorio en la pestaña **Code**.
2. Pulsa **uploading an existing file**; si ya tiene algún archivo, usa **Add file → Upload files**.
3. En el Explorador de Windows, abre **el interior** de la carpeta `ChaqRec` extraída.
4. Selecciona los archivos y carpetas del proyecto (`src`, `assets`, `docs`, `.github`, `CMakeLists.txt`, `README.md`, etc.) y arrástralos al área de GitHub.
5. Espera a que termine la carga y revisa los nombres antes de guardar.
6. En el mensaje del commit escribe: `Initial ChaqRec v0.3.0 source`.
7. Confirma en la rama **main** con **Commit changes**.

**Comprueba ahora la raíz del repositorio:** deben verse `src`, `assets`, `docs` y `CMakeLists.txt`. Si todo quedó dentro de una carpeta extra llamada `ChaqRec`, la estructura es incorrecta y el compilador no encontrará `CMakeLists.txt`.

### A3. Comprobar GitHub Actions

1. Desde **Code**, abre `.github → workflows → build.yml`.
2. Si existe, verifica que empiece con `name: Windows portable build` y continúa al apartado **Compilar y descargar `ChaqRec.exe`**.
3. Si **no** existe `.github/workflows/build.yml`, abre el apartado **Solución común** y créalo manualmente en la rama **main**. GitHub iniciará una compilación al guardar ese archivo.

---

## Opción B. Actualizar un repositorio existente

> Este procedimiento conserva `main` y sus versiones anteriores hasta que hayas compilado y probado la actualización. Ejemplo: <https://github.com/Filter27/ChaqRec>.

### B1. Proteger la versión que ya funciona

1. Abre tu repositorio original, por ejemplo <https://github.com/Filter27/ChaqRec>.
2. En **Code**, arriba de los archivos, pulsa el selector de rama **main**.
3. Escribe `feature/v0.3.0`.
4. Selecciona **Create branch: feature/v0.3.0 from main**.
5. Comprueba que ahora la rama activa sea `feature/v0.3.0` antes de subir cambios.

No borres el repositorio original, no elimines Releases anteriores y **no** borres la rama `main`.

### B2. Comprobar que el flujo ya exista en la rama principal

1. En el selector de ramas vuelve temporalmente a **main**.
2. Comprueba si existe `.github/workflows/build.yml`.
3. Si **falta**, créalo desde **Add file → Create new file**, pegando el contenido del apartado **Solución común** y guardándolo **en main**.
4. Si **ya existe** y funciona, puedes conservarlo; revisa que incluya `feature/**` en las ramas de `push` o `workflow_dispatch` para ejecutar compilaciones de la rama nueva.
5. Regresa a la rama **feature/v0.3.0**.

**Por qué crear el flujo primero en `main`:** la ejecución manual con **Run workflow** necesita que el flujo esté registrado en la rama predeterminada. Además, la opción de compilación de ramas de trabajo será visible y podrás seleccionarla después.

### B3. Actualizar los archivos en la rama de trabajo

1. En **Code**, comprueba otra vez que estás en **feature/v0.3.0**.
2. Pulsa **Add file → Upload files**.
3. Selecciona los archivos y carpetas interiores de la versión `ChaqRec-v0.3.0-source.zip` ya extraída.
4. Mantén **exactamente las mismas rutas**. Los archivos compartidos entre versiones (como `src/main.cpp`) se tienen que **reemplazar por la versión nueva**; los nuevos (como `src/Renderer.cpp` y `src/M4aRecorder.cpp`) se tienen que **agregar**.
5. Revisa el resumen de archivos modificados antes de confirmar. Si la web de GitHub no permite reemplazar un archivo o no acepta una carpeta, utiliza el **editor web** <https://github.dev/Filter27/ChaqRec> seleccionando `feature/v0.3.0`, o edita ese archivo desde GitHub usando su icono de lápiz. No sobreescribas archivos sin comprobar el resultado.
6. Guarda los cambios en esa rama con un mensaje como `Update ChaqRec to v0.3.0 source`.

**Importante:** subir archivos no elimina automáticamente archivos antiguos que ya no deberían existir. Revisa el contenido de la rama contra el ZIP. Nunca reemplaces `main` directamente para probar cambios no verificados.

### B4. Verificar los archivos y compilar sin modificar `main`

1. Comprueba que existen los nuevos archivos de `src` y que `CMakeLists.txt` tiene versión `0.3.0`.
2. Verifica `.github/workflows/build.yml` en la rama de trabajo.
3. Entra en **Actions → Windows portable build**.
4. Si aparece una ejecución automática de `feature/v0.3.0`, ábrela. Si no, pulsa **Run workflow**, elige la rama `feature/v0.3.0` y pulsa de nuevo **Run workflow**.
5. Descarga y prueba el ejecutable siguiendo los pasos comunes más adelante.

### B5. Integrar la actualización cuando funcione

1. En GitHub, entra en **Pull requests → New pull request**.
2. Selecciona **base: main** y **compare: feature/v0.3.0**.
3. Revisa los archivos que cambiarán; elige **Create pull request**.
4. Cuando el proyecto compile y funcione correctamente, usa **Merge pull request** y confirma.
5. La rama **main** contendrá entonces la v0.3.0. Conserva la versión anterior en el historial y sus Releases.

---

## Solución común: configurar GitHub Actions cuando no aparece el flujo

**Este apartado aplica a las dos opciones** y resuelve el caso en que **Actions** muestra «Get started with GitHub Actions», varias plantillas de CMake/MSBuild o el botón **New workflow**, pero no aparece **Windows portable build**.

### S1. Volver a Code

1. No elijas ninguna plantilla **C/C++ with Make**, **CMake based** ni **MSBuild**.
2. Abre la pestaña **Code** de tu repositorio.
3. Selecciona la rama **main**. Este paso es especialmente importante si actualizarás un repositorio existente.
4. Pulsa **Add file → Create new file**.

### S2. Crear la ruta exacta

En **Name your file...**, escribe **sin espacios al inicio ni al final**:

```text
.github/workflows/build.yml
```

GitHub irá creando las carpetas `.github` y `workflows` al escribir las barras `/`.

### S3. Pegar el workflow completo

Pega **todo** el siguiente contenido en el editor grande de GitHub. Está basado en el workflow que viene dentro del ZIP de ChaqRec v0.3.0:

```yaml
name: Windows portable build

on:
  push:
    branches: [main, master, 'feature/**']
    tags: ['v*']
  pull_request:
  workflow_dispatch:

permissions:
  contents: write

jobs:
  build:
    runs-on: windows-2022
    steps:
      - name: Checkout ChaqRec source
        uses: actions/checkout@v4

      - name: Checkout whisper.cpp source (MIT, pinned v1.9.5)
        uses: actions/checkout@v4
        with:
          repository: ggml-org/whisper.cpp
          ref: v1.9.5
          path: whisper-src

      - name: Configure ChaqRec (Visual Studio 2022 x64)
        run: cmake -S . -B build -G "Visual Studio 17 2022" -A x64

      - name: Build ChaqRec.exe
        run: cmake --build build --config Release --parallel

      - name: Configure offline Whisper tool (CPU only, static libraries)
        run: >-
          cmake -S whisper-src -B build-whisper -G "Visual Studio 17 2022" -A x64
          -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_EXAMPLES=ON
          -DWHISPER_BUILD_SERVER=OFF -DBUILD_SHARED_LIBS=OFF
          -DGGML_NATIVE=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded

      - name: Build whisper-cli.exe separately
        run: cmake --build build-whisper --config Release --target whisper-cli --parallel

      - name: Package portable ZIP
        shell: pwsh
        run: |
          $ErrorActionPreference = 'Stop'
          $exe = 'build/bin/Release/ChaqRec.exe'
          if (-not (Test-Path $exe)) { throw "Missing $exe" }
          $cli = Get-ChildItem 'build-whisper' -Recurse -Filter 'whisper-cli.exe' | Where-Object { $_.FullName -like '*Release*' } | Select-Object -First 1
          if (-not $cli) { throw 'Missing whisper-cli.exe' }
          New-Item -ItemType Directory -Force -Path 'portable/tools' | Out-Null
          Copy-Item $exe 'portable/ChaqRec.exe'
          Copy-Item $cli.FullName 'portable/tools/whisper-cli.exe'
          Copy-Item LICENSE 'portable/LICENSE.txt'
          Copy-Item PORTABLE-LEEME.txt 'portable/LEEME.txt'
          Copy-Item 'whisper-src/LICENSE' 'portable/tools/whisper-LICENSE.txt'
          Compress-Archive -Path 'portable/*' -DestinationPath 'ChaqRec-v0.3.0-Windows-x64-portable.zip' -Force

      - name: Upload portable executable as Artifact
        uses: actions/upload-artifact@v4
        with:
          name: ChaqRec-Windows-x64-portable
          path: ChaqRec-v0.3.0-Windows-x64-portable.zip
          if-no-files-found: error

      - name: Attach ZIP to GitHub Release for v* tags
        if: startsWith(github.ref, 'refs/tags/v')
        uses: softprops/action-gh-release@v2
        with:
          files: ChaqRec-v0.3.0-Windows-x64-portable.zip
          generate_release_notes: true
```

**Qué hace este archivo:** descarga el código de ChaqRec y whisper.cpp v1.9.5, compila `ChaqRec.exe` y `whisper-cli.exe` para Windows x64 (el modelo de 148 MB NO se incluye en el ZIP), empaqueta ambos y sube el ZIP como Artifact. También adjunta el paquete a una Release al crear una etiqueta `v*` si la compilación de esa etiqueta termina bien.

### S4. Guardar y verificar

1. Pulsa **Commit changes...**.
2. Escribe el mensaje `Add ChaqRec v0.3.0 Windows build`.
3. Elige **Commit directly to the main branch**.
4. Confirma con **Commit changes**.
5. Regresa a **Code → .github → workflows → build.yml** y verifica que puedas abrirlo.
6. Abre **Actions**. Ahora debería aparecer **Windows portable build**. Puede arrancar automáticamente por el `push` a `main`.

> Si ya existía `.github/workflows/build.yml`, **no crees un segundo archivo con el mismo nombre**. Ábrelo, pulsa el lápiz y modifica su contenido solo si necesita actualizarse.

---

## Compilar y descargar `ChaqRec.exe`

### C1. Iniciar la compilación

1. Abre la pestaña **Actions** del repositorio.
2. En el menú izquierdo selecciona **Windows portable build**.
3. Busca la ejecución más reciente; tras un commit a `main`, normalmente se inicia automáticamente.
4. Si no aparece ninguna ejecución, selecciona **Run workflow**, elige la rama correspondiente (`main` para repositorio nuevo, `feature/v0.3.0` para actualización en prueba) y confirma con **Run workflow**.
5. Abre la ejecución para consultar los pasos del trabajo **build**.

Los estados más comunes son:

- ✅ **Success:** la compilación y el empaquetado terminaron correctamente.
- 🟡 **In progress / Queued:** la ejecución todavía no termina.
- ❌ **Failure:** ocurrió un error; consulta el apartado de diagnóstico.

### C2. Descargar el Artifact

1. Abre una ejecución con indicador verde.
2. En el resumen, baja hasta **Artifacts**.
3. Descarga el Artifact llamado **`ChaqRec-Windows-x64-portable`**.
4. Extrae el **primer ZIP** descargado de GitHub Actions.
5. Dentro encontrarás otro ZIP llamado **`ChaqRec-v0.3.0-Windows-x64-portable.zip`**.
6. Extrae ese **segundo ZIP**.
7. Abre la carpeta resultante. Su contenido debe incluir:

```text
ChaqRec.exe
tools/whisper-cli.exe
LICENSE.txt
LEEME.txt
```

**Ese `ChaqRec.exe` es el ejecutable portable de Windows x64.** No confundas el Artifact ni el ZIP fuente con el `.exe`. Una compilación correcta no garantiza que toda la aplicación funcione: debe probarse antes de publicarla.

### C3. Si el Artifact no aparece

- Asegúrate de que el estado de la ejecución es **Success**.
- Abre el trabajo **build** y verifica el paso **Upload executable (download from Actions / Artifacts)**.
- Si el empaquetado o la subida fallaron, consulta el primer error relevante del registro.

---

## Probar la nueva versión

Antes de compartir ChaqRec, abre `ChaqRec.exe` en tu Windows y marca cada prueba:

- [ ] El programa se abre y el visualizador aparece.
- [ ] El espectro responde a la música o al audio del sistema.
- [ ] Puedes mover, redimensionar y bloquear el visualizador.
- [ ] Puedes ajustar sensibilidad de 1 a 10 y suavizado.
- [ ] Se muestra el color azul oscuro y cambia la opacidad.
- [ ] El visualizador no muestra la palabra `REC`.
- [ ] El icono de la bandeja ofrece las opciones de grabar y detener.
- [ ] Graba un minuto en M4A y el archivo se reproduce correctamente.
- [ ] Graba 30 y 60 minutos sin interrupciones ni errores al cerrar.
- [ ] La grabación WAV opcional funciona correctamente.
- [ ] En **Audio del visualizador → Ambos**, responde tanto al micrófono como al audio de Windows.
- [ ] Al pulsar **Win+D** con **Siempre visible**, el espectro reaparece y al pulsar **Ocultar**, no.
- [ ] **Acerca de** abre el GitHub y el enlace de PayPal.
- [ ] **Solo grabar** no descarga el modelo Whisper.
- [ ] **Solo transcribir** informa del modelo gratuito y pide confirmación antes de habilitarse.
- [ ] Se descargan y validan ~148 MB del modelo en el primer uso con transcripción.
- [ ] Tras detener la grabación se crea un archivo TXT y funciona **Abrir última transcripción**.
- [ ] En **Solo transcribir**, el audio se elimina únicamente si se generó correctamente el TXT.
- [ ] En **Grabar y transcribir**, se conservan el audio y el TXT.
- [ ] En una segunda transcripción, el modelo ya descargado funciona sin conexión.
- [ ] El consumo de RAM y CPU sigue siendo ligero durante la grabación; Whisper consume recursos solo al transcribir.

Las grabaciones deben aparecer en **Documentos → ChaqRec → Recordings**.

**Limitación actual:** la captura de grabaciones usa el **micrófono**; no garantiza grabar las voces remotas de Teams, Zoom u otras aplicaciones. El modelo local también requiere validación en Windows real. **Ambos** mezcla visualización, NO modifica la captura del micrófono.

---

## Publicar una versión para descargas

### D1. Crear la Release

1. Verifica que el código probado esté en la rama **main**. Si seguiste la **Opción B**, primero termina el Pull Request y fusiona la rama de trabajo.
2. Abre **Releases** desde la página del repositorio.
3. Pulsa **Draft a new release**.
4. En **Choose a tag**, escribe `v0.3.0` y elige crear una etiqueta nueva desde `main`.
5. En **Release title**, escribe `ChaqRec v0.3.0`.
6. Escribe un resumen breve de las mejoras y pulsa **Publish release**.

### D2. Adjuntar la descarga

El `build.yml` incluido intenta adjuntar automáticamente el archivo **`ChaqRec-v0.3.0-Windows-x64-portable.zip`** a la Release cuando se ejecuta con una etiqueta `v*`. Consulta **Actions** para ver si la ejecución termina correctamente y después revisa **Releases → Assets**.

Si la subida automática no se completa, entra en **Edit release**, arrastra **el ZIP portable que ya descargaste y probaste** al recuadro **Attach binaries** y pulsa **Update release**.

**No adjuntes como ejecutable** el ZIP de código fuente `ChaqRec-v0.3.0-source.zip`.

---

## Diagnóstico de errores frecuentes

| Lo que aparece | Posible motivo | Solución |
|---|---|---|
| «Get started with GitHub Actions» | Falta el archivo workflow en la rama principal | Sigue el apartado **Solución común** y crea `.github/workflows/build.yml` en `main`. |
| Solo aparecen plantillas CMake / MSBuild | No hay un workflow de ChaqRec registrado | **No selecciones una plantilla**; crea el workflow específico. |
| «Run workflow» no aparece | El workflow no está en la rama principal, o carece de `workflow_dispatch` | Guarda el workflow en `main` y revisa el contenido. |
| `CMake Error: ... CMakeLists.txt` | Subiste una carpeta `ChaqRec` adicional y el archivo no quedó en la raíz | Mueve o vuelve a subir el contenido interior del ZIP a la raíz. |
| `error C...`, `fatal error`, `LNK...` | Error en el código o en la compilación | Abre **Actions → ejecución → build** y copia el primer error con sus líneas anteriores. |
| `Missing whisper-cli.exe` | No terminó la compilación auxiliar de Whisper | Revisa `Checkout whisper.cpp`, `Configure offline Whisper tool` y `Build whisper-cli.exe separately`. |
| No se genera TXT | Falta `tools/whisper-cli.exe`, falla descarga modelo o conversión Media Foundation | Prueba usando el ZIP portable **completo** y revisa el error de ChaqRec. |
| `Missing: build/bin/Release/ChaqRec.exe` | No se generó el ejecutable en la ruta esperada | Inspecciona los pasos de configuración y compilación. |
| No aparece el Artifact | La ejecución falló o el paso Upload no se completó | Consulta los registros del trabajo. |
| El `.exe` existe, pero no graba M4A | Error de ejecución, dispositivo o codificador | Prueba el modo WAV y aporta el mensaje de error; es una función que requiere validación. |
| Aviso sobre versión de Node.js con ✅ verde | Aviso de compatibilidad de acciones | No implica necesariamente que la compilación haya fallado. |

### Cómo enviar un error útil para corregirlo

1. Abre **Actions** y selecciona la ejecución con ❌.
2. Entra en **build**.
3. Abre el **primer paso rojo**.
4. Copia el **primer error real** y unas 10 líneas alrededor (no basta una captura de la X roja).
5. Indica si estabas compilando `main` o `feature/v0.3.0`.

---

## Enlaces rápidos

- Crear repositorio: <https://github.com/new>
- Repositorio de ejemplo original: <https://github.com/Filter27/ChaqRec>
- Actions del repositorio original: <https://github.com/Filter27/ChaqRec/actions>
- Editor web de GitHub: <https://github.dev/Filter27/ChaqRec>
- Documentación de GitHub (agregar archivos): <https://docs.github.com/es/repositories/working-with-files/managing-files/adding-a-file-to-a-repository>
- Ejecutar un workflow manualmente: <https://docs.github.com/es/actions/how-tos/manage-workflow-runs/manually-run-a-workflow>
- Descargar Artifacts: <https://docs.github.com/es/actions/how-tos/manage-workflow-runs/download-workflow-artifacts>

---

## Si quieres reemplazar todo el contenido de `Filter27/ChaqRec`

No necesitas borrar el repositorio entero ni perder tus Releases, Issues o el historial. Para dejar la **misma URL** y reemplazar archivos:

1. **Respalda el código anterior:** **Code → Download ZIP** y guarda una copia fuera del proyecto.
2. Crea una rama `feature/v0.3.0` desde `main` (Opción B).
3. Abre la rama en [github.dev](https://github.dev/Filter27/ChaqRec), elimina **únicamente los archivos de código antiguos que ya no correspondan**, y sube los nuevos desde el ZIP. Conserva `.github/workflows/build.yml` con el YAML **actualizado**.
4. Comprueba que `CMakeLists.txt`, `src/`, `assets/`, `.github/` estén en la raíz; no dejes una segunda carpeta `ChaqRec/`.
5. Haz **Commit** y **Sync Changes** en github.dev; revisa **Actions** en la rama, descarga el portable y pruébalo.
6. Solo cuando funcione, haz Pull Request hacia `main` y fusiónalo.

**No utilices Settings → Delete this repository** para esta actualización: eliminarías también historial, Issues, Releases y configuraciones de GitHub. Si el navegador rechaza sobrescribir un archivo, elimínalo y vuelve a cargarlo en la **rama de trabajo**, nunca borres el repositorio completo.

---

## Resumen de ambas opciones

**Opción A — Desde cero:** extraer ZIP → crear repositorio nuevo → subir archivos interiores → comprobar o crear `.github/workflows/build.yml` → Actions → descarga del Artifact → extraer dos ZIP → probar el `.exe` → publicar Release.

**Opción B — Actualizar:** conservar `main` → comprobar o crear workflow en `main` → crear rama `feature/v0.3.0` → subir allí el código nuevo → Actions en esa rama → probar `.exe` → Pull Request → fusionar en `main` → publicar Release.
