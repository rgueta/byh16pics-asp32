#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <by_config.h>
#include <esp_heap_caps.h>
#include <WiFiClientSecure.h>

WiFiClientSecure httpsClient;

// ======  Boton  =============
// #define BUTTON_PIN 16
const int botonPin = 13; 
bool botonPresionado = false;
bool ultimoEstadoBoton = HIGH;
unsigned long ultimoTiempoDebounce = 0;
unsigned long delayDebounce = 200;


// ========= Socket TCP   ===============================
WiFiServer server(1234);
WiFiClient tcpClient;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  // ========== Botones   ===================
  pinMode(botonPin, INPUT_PULLUP);
  // attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonIsr, FALLING);


   // Configurar cliente seguro para Telegram
  httpsClient.setInsecure(); // Evita problemas con certificados HTTPS(SSL)

   // Configurar la dirección IP estática
  if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Fallo al configurar la IP estática");
  }

  pinMode(4, OUTPUT); // Flash
  digitalWrite(4, LOW);

    // Parpadear LED para indicar inicio
  for(int i = 0; i < 3; i++) {
    digitalWrite(4, HIGH);
    delay(200);
    digitalWrite(4, LOW);
    delay(200);
  }

  WiFi.begin(ssid, password);
  Serial.println("Conectando WiFi...");
  
  // Conectar a WiFi =================
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  { 
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {

    if (httpsClient.connect("api.telegram.org", 443)) {
      // Enviar mensaje de inicio a Telegram
      enviarTelegram("🚀 ESP32-CAM Reiniciado y Listo! IP: " + WiFi.localIP().toString());
      Serial.println("✅ Notificación enviada a Telegram!");
    } else {
      Serial.println("❌ No se pudo conectar a Telegram.");
    }

    Serial.println("✅ WiFi Conectado, 🌐 IP: " + WiFi.localIP().toString());
    // OBTENER POTENCIA DE SEÑAL
    int rssi = WiFi.RSSI();
    String calidad = getSignalQuality(rssi);
    Serial.println("📶 Potencia señal WiFi " + calidad + ": " + String(rssi) + " dBm");
  }else {
    Serial.println("\n❌ Error conexión WiFi");
  }

  server.begin();
  Serial.println("Servidor TCP iniciado en puerto 1234");


   // Inicializar cámara
  if (!startCamera()) {
    Serial.println("❌ Error inicializando cámara");
    return;
  }else{
    Serial.println("✅ Cámara inicializada correctamente");
  }

}

void loop() {
   // Manejar nueva conexión TCP
  if (!tcpClient || !tcpClient.connected()) {
    tcpClient = server.available();
    if (tcpClient) {
      Serial.println("Cliente TCP conectado");
      tcpClient.println("ESP32-CAM Listo. Envia comandos:");
    }
  }

    // Procesar datos del cliente TCP
  if (tcpClient && tcpClient.available()) {
    String input = tcpClient.readStringUntil('\n');
    input.trim();
    input.toUpperCase();

    if (input.length() > 0) {
      Comando cmd = parsearComando(input);
      ejecutarComando(cmd);
    }
  }

    // ======  Botones  ==============
  //  Leer estado del botón con debounce50
  int lecturaBoton = digitalRead(botonPin);
  
  if (lecturaBoton != ultimoEstadoBoton) {
    ultimoTiempoDebounce = millis();
  }
  
  if ((millis() - ultimoTiempoDebounce) > delayDebounce) {
    if (lecturaBoton == LOW && !botonPresionado) {
      botonPresionado = true;
      // tomarYEnviarFoto();
      Serial.println("Boton presionado!");
    } else if (lecturaBoton == HIGH) {
      botonPresionado = false;
    }
  }
  
  ultimoEstadoBoton = lecturaBoton;
  delay(10);
}




// ================== FUNCIONES DE TELEGRAM ==================

void enviarTelegram(String mensaje) {
   String url = String("/bot") + TELEGRAM_BOT_TOKEN + "/sendMessage?chat_id=" + TELEGRAM_CHAT_ID + "&text=" + mensaje;
   Serial.println("enviarTelegram url: " + url);
   httpsClient.print(String("GET ") + url + " HTTP/1.1\r\nHost: api.telegram.org\r\nConnection: close\r\n\r\n");
  
}

// ================== FUNCIONES DE TCP  ==================
// Función para parsear comandos tcp
Comando parsearComando(String input) {
  Comando cmd;
  cmd.numParametros = 0;
  
  // Convertir a mayúsculas y trim
  input.toUpperCase();
  input.trim();
  
  // Encontrar primer espacio para separar nombre de parámetros
  int firstSpace = input.indexOf(' ');
  
  if (firstSpace == -1) {
    // Solo hay nombre, sin parámetros
    cmd.nombre = input;
    return cmd;
  }
  
  // Separar nombre
  cmd.nombre = input.substring(0, firstSpace);
  String resto = input.substring(firstSpace + 1);
  
  // Parsear parámetros separados por espacios o comas
  int start = 0;
  int end = 0;
  
  while (end != -1 && cmd.numParametros < 5) {
    // Buscar próximo espacio o coma
    int spacePos = resto.indexOf(' ', start);
    int commaPos = resto.indexOf(',', start);
    
    // Usar el separador que aparezca primero
    if (spacePos == -1 && commaPos == -1) {
      // Último parámetro
      end = -1;
      cmd.parametros[cmd.numParametros++] = resto.substring(start);
    } else if (spacePos != -1 && (commaPos == -1 || spacePos < commaPos)) {
      end = spacePos;
    } else {
      end = commaPos;
    }
    
    if (end != -1) {
      cmd.parametros[cmd.numParametros++] = resto.substring(start, end);
      start = end + 1;
      
      // Saltar múltiples espacios/comas
      while (start < resto.length() && (resto.charAt(start) == ' ' || resto.charAt(start) == ',')) {
        start++;
      }
    }
  }
  
  return cmd;
}

// Función para ejcutar comandos tcp
void ejecutarComando(Comando cmd) {
   Serial.println("Ejecutando comando: " + cmd.nombre + " (" + String(cmd.numParametros) + " params)");
  if (cmd.nombre == "SET_RESOLUTION") {
    if (cmd.numParametros >= 1) {
      // setResolution(cmd.parametros[0].toInt());
      Serial.println("setResolution param: " + cmd.parametros[0]);
      enviarRespuesta("setResolution param: " + cmd.parametros[0]);
    }
  } else if (cmd.nombre == "SET_QUALITY") {
    if (cmd.numParametros >= 1) {
      // setQuality(cmd.parametros[0].toInt());
      Serial.println("setQuality param: " + cmd.parametros[0]);
    }
  } else if (cmd.nombre == "CAPTURE") {
    tomarYEnviarFoto();

  } else if (cmd.nombre == "HELP") {
    mostrarAyuda();
  } else if (cmd.nombre == "RSSI") {
    int rssi = WiFi.RSSI();
    String calidad = getSignalQuality(rssi);
    enviarRespuesta("📶 Potencia señal WiFi " + calidad + ": " + String(rssi) + " dBm");

  }else if (cmd.nombre == "FLASH") {
    manejarFlash(cmd);

  }  else if (cmd.nombre == "MAC") {
    enviarRespuesta("MAC: " + WiFi.macAddress());

  }else if (cmd.nombre == "MEMORY" || cmd.parametros[0] == "RAM") {
      if (cmd.numParametros >= 1) {
        String tipo = cmd.parametros[0];
        if (tipo == "DRAM") {
          getDRAMMemory();
        } else if (tipo == "PSRAM") {
          getPSRAMMemory();
        } else if (tipo == "INTERNAL") {
          getInternalMemory();
        } else if (tipo == "FRAG" || tipo == "FRAGMENTATION") {
          getMemoryFragmentation();
        } else if (tipo == "ALL" || tipo == "FULL") {
          getAllMemoryInfo();
        } else {
          enviarRespuesta("❌ Tipo de memoria no válido");
        }
    } else {
      // Comando MEMORY sin parámetros - info básica
      enviarRespuesta("💾 Memoria libre: " + String(esp_get_free_heap_size()) + " bytes");
      enviarRespuesta("💾 PSRAM: " + String(psramFound() ? "Disponible" : "No disponible"));
    }

  }else if (cmd.nombre == "UPTIME") {
      enviarRespuesta("⏰ Uptime: " + String(millis() / 1000) + " segundos");

  } else if (cmd.nombre == "RESET" || cmd.nombre == "REBOOT") {
    enviarRespuesta("🔄 Reiniciando ESP32-CAM...");
    delay(1000);
    ESP.restart();
  }

}

// Función para enviar respuesta al cliente TCP
void enviarRespuesta(String mensaje) {
  if (tcpClient && tcpClient.connected()) {
    tcpClient.println(mensaje);
    tcpClient.flush(); // Asegurar que se envía inmediatamente
  }
  // También mostrar en Serial para debug
  Serial.println("TCP >> " + mensaje);
}

// Mostrar ayuda
void mostrarAyuda() {
  String ayuda = "Comandos disponibles:\n";
  ayuda += "  FOTO - Tomar una foto\n";
  ayuda += "  FLASH [ON|OFF|TOGGLE] - Controlar flash\n";
  ayuda += "  SET QUALITY 0-100 - Configurar calidad\n";
  ayuda += "  SET BRIGHTNESS -2-2 - Configurar brillo\n";
  ayuda += "  SET RESOLUTION valor - Configurar resolución\n";
  ayuda += "  STATUS [FLASH|MEMORY|UPTIME] - Ver estado\n";
  ayuda += "  HELP - Mostrar esta ayuda\n";
  ayuda += "  RESET - Reiniciar dispositivo";
  enviarRespuesta(ayuda);
}

// ================== FUNCIONES DE MONITOREO DE MEMORIA ==================

// Obtener estadísticas de memoria DRAM
void getDRAMMemory() {
  enviarRespuesta("=== MEMORIA DRAM (Data RAM) ===");
  
  size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t total_dram = heap_caps_get_total_size(MALLOC_CAP_8BIT);
  size_t used_dram = total_dram - free_dram;
  size_t min_free_dram = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
  
  enviarRespuesta("Total: " + String(total_dram) + " bytes (" + String(total_dram/1024) + " KB)");
  enviarRespuesta("Usada: " + String(used_dram) + " bytes (" + String(used_dram/1024) + " KB)");
  enviarRespuesta("Libre: " + String(free_dram) + " bytes (" + String(free_dram/1024) + " KB)");
  enviarRespuesta("Mínimo libre: " + String(min_free_dram) + " bytes");
  enviarRespuesta("Uso: " + String((used_dram * 100) / total_dram) + "%");
}

// Obtener estadísticas de memoria PSRAM
void getPSRAMMemory() {
  enviarRespuesta("=== MEMORIA PSRAM (Pseudo Static RAM) ===");
  
  if (psramFound()) {
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t used_psram = total_psram - free_psram;
    size_t min_free_psram = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    
    enviarRespuesta("Total: " + String(total_psram) + " bytes (" + String(total_psram/1024) + " KB)");
    enviarRespuesta("Usada: " + String(used_psram) + " bytes (" + String(used_psram/1024) + " KB)");
    enviarRespuesta("Libre: " + String(free_psram) + " bytes (" + String(free_psram/1024) + " KB)");
    enviarRespuesta("Mínimo libre: " + String(min_free_psram) + " bytes");
    enviarRespuesta("Uso: " + String((used_psram * 100) / total_psram) + "%");
  } else {
    enviarRespuesta("❌ PSRAM no disponible en este dispositivo");
  }
}

// Obtener estadísticas de memoria interna
void getInternalMemory() {
  enviarRespuesta("=== MEMORIA INTERNA ===");
  
  size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t total_internal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
  size_t used_internal = total_internal - free_internal;
  
  enviarRespuesta("Total: " + String(total_internal) + " bytes (" + String(total_internal/1024) + " KB)");
  enviarRespuesta("Usada: " + String(used_internal) + " bytes (" + String(used_internal/1024) + " KB)");
  enviarRespuesta("Libre: " + String(free_internal) + " bytes (" + String(free_internal/1024) + " KB)");
  enviarRespuesta("Uso: " + String((used_internal * 100) / total_internal) + "%");
}

// Obtener fragmentación de memoria
void getMemoryFragmentation() {
  enviarRespuesta("=== FRAGMENTACIÓN DE MEMORIA ===");
  
  size_t free_size = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  
  float fragmentation = 0;
  if (free_size > 0) {
    fragmentation = (1.0 - (float)largest_free_block / free_size) * 100.0;
  }
  
  enviarRespuesta("Bloque libre más grande: " + String(largest_free_block) + " bytes");
  enviarRespuesta("Fragmentación: " + String(fragmentation, 2) + "%");
  
  if (fragmentation > 50) {
    enviarRespuesta("⚠️  Alta fragmentación - considerar reinicio");
  } else if (fragmentation > 30) {
    enviarRespuesta("⚠️  Fragmentación moderada");
  } else {
    enviarRespuesta("✅ Fragmentación baja");
  }
}

// Obtener todos los tipos de memoria
void getAllMemoryInfo() {
  enviarRespuesta("=== INFORMACIÓN COMPLETA DE MEMORIA ===");
  
  // Memoria DRAM
  size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t total_dram = heap_caps_get_total_size(MALLOC_CAP_8BIT);
  
  // Memoria interna
  size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t total_internal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
  
  // PSRAM
  size_t free_psram = 0;
  size_t total_psram = 0;
  bool has_psram = psramFound();
  if (has_psram) {
    free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  }
  
  enviarRespuesta("DRAM: " + String(free_dram) + "/" + String(total_dram) + " bytes libres");
  enviarRespuesta("Interna: " + String(free_internal) + "/" + String(total_internal) + " bytes libres");
  
  if (has_psram) {
    enviarRespuesta("PSRAM: " + String(free_psram) + "/" + String(total_psram) + " bytes libres");
  } else {
    enviarRespuesta("PSRAM: No disponible");
  }
  
  // Fragmentación
  size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  float frag = (1.0 - (float)largest_block / free_dram) * 100.0;
  enviarRespuesta("Fragmentación DRAM: " + String(frag, 2) + "%");
}

String getSignalQuality(int rssi) {
  if (rssi >= -50) return "Excelente 🔥";
  else if (rssi >= -60) return "Muy Buena ✅";
  else if (rssi >= -70) return "Buena 👍";
  else if (rssi >= -80) return "Regular ⚠️";
  else return "Mala ❌";
}

// Manejar comandos de flash
void manejarFlash(Comando cmd) {
  if (cmd.numParametros >= 1) {
    if (cmd.parametros[0] == "ON" || cmd.parametros[0] == "1") {
      digitalWrite(4, HIGH);
      enviarRespuesta("💡 Flash ENCENDIDO");
    } 
    else if (cmd.parametros[0] == "OFF" || cmd.parametros[0] == "0") {
      digitalWrite(4, LOW);
      enviarRespuesta("💡 Flash APAGADO");
    }
    else if (cmd.parametros[0] == "TOGGLE" || cmd.parametros[0] == "T") {
      bool estado = !digitalRead(4);
      digitalWrite(4, estado);
      enviarRespuesta("💡 Flash: " + String(estado ? "ENCENDIDO" : "APAGADO"));
    }
    else {
      enviarRespuesta("❌ ERROR: Uso: FLASH [ON|OFF|TOGGLE]");
    }
  } else {
    enviarRespuesta("💡 Flash actual: " + String(digitalRead(4) ? "ENCENDIDO" : "APAGADO"));
  }
}

// ================== FUNCIONES DE CAMARA ==================
bool startCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
    // Calidad de imagen
  config.frame_size = FRAMESIZE_SVGA; // 800x600
  config.jpeg_quality = 10; // 0-63 (menor = mejor calidad)
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error cámara: 0x%x", err);
    return false;
  }

  cameraInitialized = true;
  return true;
}

void tomarYEnviarFoto() {
  // Capturar foto
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Error capturando foto");
    return;
  }
  
  Serial.printf("Foto capturada - Tamaño: %zu bytes\n", fb->len);
  
  // Enviar al servidor
  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("X-Device-ID", "ESP32-CAM-01"); // Identificador único
  
  int httpResponseCode = http.POST(fb->buf, fb->len);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("Código HTTP: %d\n", httpResponseCode);
    Serial.printf("Respuesta: %s\n", response.c_str());
  } else {
    Serial.printf("Error en HTTP: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  
  http.end();
  esp_camera_fb_return(fb);
}
