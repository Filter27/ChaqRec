# Pruebas manuales — ChaqRec 0.3.0 (Windows 10/11 x64)

> Estos puntos NO se han validado automáticamente en Windows al generar el ZIP fuente. Marcar solo después de probar el ejecutable descargado de GitHub Actions.

## Compilación y portabilidad

- [ ] GitHub Actions → Windows portable build → `Success`.
- [ ] Artifact contiene `ChaqRec-v0.3.0-Windows-x64-portable.zip`.
- [ ] Descompresión produce `ChaqRec.exe`, `tools/whisper-cli.exe`, `LICENSE.txt` y `LEEME.txt`.
- [ ] Se abre ChaqRec.exe en un Windows 10/11 x64 sin Visual Studio.

## Visualizador y ventanas

- [ ] Las tres formas y seis colores funcionan; opacidad y sensibilidad se guardan.
- [ ] Fuente `Sonido de Windows`: responde a música/video.
- [ ] Fuente `Micrófono`: responde a voz con salida de Windows en silencio.
- [ ] Fuente `Ambos`: responde a voz *y* música independientemente.
- [ ] Activar `Siempre visible`, pulsar **Win + D** y observar si reaparece el espectro sin click.
- [ ] Elegir `Ocultar`, pulsar **Win + D** y verificar que **NO reaparezca**.
- [ ] Desactivar `Siempre visible` y comprobar respeto a la opción.
- [ ] Desbloquear/bloquear y mover/redimensionar.
- [ ] `Acerca de` muestra enlaces y abre el navegador en GitHub y PayPal.

## Grabar / transcribir

- [ ] `Solo grabar`: graba 1 min M4A; se reproduce; no descarga modelo.
- [ ] `Solo grabar`: graba WAV; se reproduce.
- [ ] Elegir `Solo transcribir`: se informa el tamaño (~148 MB) y uso local; `No` cancela.
- [ ] Elegir `Grabar y transcribir`: se informa lo mismo.
- [ ] Descargar modelo la primera vez; verificar `%LOCALAPPDATA%\ChaqRec\Models\ggml-base.bin`.
- [ ] Transcribir audio de prueba en español; se crea TXT no vacío junto a audio.
- [ ] `Solo transcribir`: audio se elimina **solo tras completar TXT**.
- [ ] `Grabar y transcribir`: quedan M4A + TXT.
- [ ] Tras descargar el modelo, transcribir sin internet.
- [ ] Si el modelo no está disponible o falla la transcripción, no se elimina el audio.
- [ ] Tras transcribir, `whisper-cli.exe` no queda abierto; el uso de RAM vuelve al nivel base.
- [ ] Grabar y transcribir una reunión larga (30–60 min).
- [ ] Cerrar ChaqRec durante transcripción y comprobar que no quedan procesos huérfanos.
- [ ] Probar sin micrófono habilitado y con cambio de dispositivo.

## Limitación conocida

`Ambos` mezcla **solo los datos visuales** del espectro. La grabación original siempre proviene del **micrófono**, no del audio de las reuniones remotas.
