#include <Time.h>
#include <TimeAlarms.h>
#include <SD.h>
#include <SPI.h>         
#include <Ethernet.h>


byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x6A, 0x2F };
IPAddress my_ip(192, 168, 0, 2);
IPAddress server_ip(192, 168, 0, 112);
int server_port = 7999;
int LED_PIN = 9;
int SD_PIN = 4;


/************************************************
 *  Main Arduino methods
 ***********************************************/

void setup() 
{
  Serial.begin(9600);
  while (!Serial);
  
  Serial.println("\n\nInitializing...");
  pinMode(LED_PIN, OUTPUT);
  pinMode(SD_PIN, OUTPUT);
  pinMode(SS_PIN, OUTPUT);
  setSyncProvider(get_timestamp_from_server);
  
  while(timeStatus() == timeNotSet);

  init_sd();
 
  Alarm.timerRepeat(5, dump_data_from_sensors);  // every 5 seconds
//  Alarm.timerRepeat(5 * 60, dump_data_from_sensors);  // every 5 minutes

  Serial.println("STARTED");
}


void loop()
{
  Alarm.delay(15000);  // how much should this be? I don't have to do anything O.o
}




 

/************************************************
 *  SD METHODS
 ***********************************************/
 
int init_sd() { 
  digitalWrite(SD_PIN, LOW);
  
  if (!SD.begin(SD_PIN)) {
    Serial.println("SD initialization failed!");
    return 0;
  }
  return 1;
}

void get_next_filename(char* filename_buf, int filename_buf_size) {
  // generates the next non-existing filename, ready to be created and written.
  // WARNING: names must be short 8.3 (that's why we have to use this method btw)
  time_t next_file_counter = now();
  String filename;
  String counter_str;
  do {
    counter_str = String(next_file_counter++);
    filename = counter_str.substring(counter_str.length() - 8) + ".TXT";
    filename.toCharArray(filename_buf, filename_buf_size);
  } while (SD.exists(filename_buf));
}


void dump_data_from_sensors() {

  char filename_buf[13];
  get_next_filename(filename_buf, 13);

  Serial.print("writing ");
  Serial.print(filename_buf);
  
  File fout = SD.open(filename_buf, FILE_WRITE);
  if (fout) {
    fout.print("timestamp=");
    fout.print(now());
    fout.print("&temperature=");
    fout.print("26.8");
    fout.println();
    fout.close();
    Serial.println("\tOK");    
  } else {
    Serial.println("\tUnable to write the file");
  }
  
  list_directory_and_post_to_server();
}


void list_directory_and_post_to_server() {
  File dir = SD.open("/");
  if (!dir) {
    Serial.println("Directory doesn't exist O.o");
  }

  dir.rewindDirectory();
  while(File entry = dir.openNextFile()) {    
    String filename = String(entry.name());
    String data = "";
    boolean success = false;
    if (filename.endsWith(".TXT")) {
      Serial.println(filename);
      while (entry.available()) {
        char c = entry.read();
        data += c;
      }
    }
    entry.close();
    
    if (data)
//      success = post_to_server(data);
      Serial.println(data);
    
    if (success)
      delete_file(filename);
  }
  dir.close();
}


boolean delete_file(String filename) {
  char filename_buf[13];
  filename.toCharArray(filename_buf, 13);
  if (SD.exists(filename_buf)) {
    return SD.remove(filename_buf);
  }
  return false;
}





/************************************************
 *  ETHERNET METHODS
 ***********************************************/

void init_ethernet() {
  // disable SD
  digitalWrite(SD_PIN, HIGH);

  Ethernet.begin(mac, my_ip);
}


boolean post_to_server(String data) {
  EthernetClient client;
  String line;
  digitalWrite(SD_PIN, HIGH);

  Serial.print("sending to the server ");
  if (client.connect(server_ip, server_port)) {

    Serial.print("\tconnected!");
    client.println("POST /arduino-post/ HTTP/1.0");
    client.println("User-Agent: arduino-ethernet-board");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(data.length());  // remove \r\n
    client.println();
    
    client.print(data);
    client.println();

    boolean success = false;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        line += c;
        if (c == '\n') {
          if (line.indexOf("200 OK") >= 0) {
            success = true;
            break;
          }
          line = "";
        }
      }
    }
    client.stop();
    Serial.print("\tdisconnected with result: ");
    Serial.println(success);
  digitalWrite(SD_PIN, LOW);
    return success;
    
  } else {
    Serial.println("\tconnection failed -.-");
    return false;
  }
}





/************************************************
 *  TIME METHODS
 ***********************************************/

time_t get_timestamp_from_server() {
  EthernetClient client;
  String line = "";

  init_ethernet();
  
  Serial.print("asking current timestamp to the server...");
  if (client.connect(server_ip, server_port)) {

    Serial.print("\tconnected!");
    client.println("GET /arduino-timestamp/ HTTP/1.0");
    client.println("User-Agent: arduino-ethernet-board");
    client.println();

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        line += c;
        if (c == '\n') {
          line = "";
        }
      }
    }
    client.stop();
    Serial.println("\tdisconnected");
    return line.toInt();
    
  } else {
    Serial.println("\tconnection failed -.-");
    return 0;
  }
}


