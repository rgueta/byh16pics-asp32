// ======= CONFIGURACIONES =======
struct Comando {
  String nombre;
  String parametros[5];
  int numParametros;
};

bool cameraInitialized = false;
// Telegram bot TOKEN
const char* TELEGRAM_BOT_TOKEN = "8495116756:AAFJFvdJQH7LZ5-vQRL4YLYwUjXRUq8hjtQ";
const char* TELEGRAM_CHAT_ID= "405240137";

// Configuración de IP estática
IPAddress staticIP(192, 168, 1, 64); // Esta es la ultima ip del rango disponible 
IPAddress gateway(192, 168, 1, 254);    // IP de tu router
IPAddress subnet(255, 255, 255, 0);  // Máscara de subred
IPAddress primaryDNS(8, 8, 8, 8);    // Opcional: DNS primario de Google
IPAddress secondaryDNS(8, 8, 4, 4);  // Opcional: DNS secundario de Google


// Configuración servidor Node.js
const char* serverURL = "http://192.168.1.185:3000/upload";

const char* ssid = "FamGuEst_2.4";
const char* password = "l0l1t@..:)";

//======= Pines del módulo AI Thinker =======
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22