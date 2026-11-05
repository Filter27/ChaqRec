# Historial de versiones

## v0.3.0 — Código fuente de prueba

- «Acerca de» con navegación a GitHub Filter27/ChaqRec y donaciones PayPal.
- Watchdog ligero para reponer el espectro tras Win+D cuando «Siempre visible» está activo, respetando «Ocultar».
- Fuente de visualización «Ambos» = WASAPI loopback + micrófono; análisis FFT individual sin mezclar la captura de grabación.
- Opciones de grabación: solo audio; solo transcribir TXT; grabar y transcribir.
- Transcripción de audio M4A/WAV local mediante Whisper.cpp empaquetado como ejecutable secundario, únicamente después de grabar.
- Consentimiento para descargar modelo multilingüe ggml-base de ~148MB; verificación por SHA-256 y descarga reutilizable desde LocalAppData.
- Conversión de audio con Media Foundation a WAV 16kHz; ejecución Whisper aparte para preservar el consumo normal de ChaqRec.
- GitHub Actions compila ChaqRec y whisper-cli CPU; se conservan las opciones v0.2.0.
- Requiere validar compilación y funciones sobre Windows real antes de etiquetar estable.

## v0.2.0

- Visualizador GDI+ ARGB con estilos fluidos y ajustes de sensibilidad/opacidad.
- Grabación AAC/M4A y PCM/WAV, indicador rojo y colores azul oscuro/eléctrico.

## v0.1.0

- Grabación WAV, capturas WASAPI, visualizador nativo y GitHub Actions.
