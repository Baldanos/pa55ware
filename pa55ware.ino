#include <EEPROM.h>
#include <SD.h>
#include <Time.h>  
#include <SPI.h>
#include <Teensy3_ST7735.h> // Hardware-specific library

#include <AES.h> // Courtesy of Brian Gladman, Worcester, UK

#include <sha1.h> //From cryptosuite https://github.com/Cathedrow/Cryptosuite.git
                  //Patched with http://bazaar.launchpad.net/~chuck-bell/mysql-arduino/trunk/view/head:/sha1.diff



/*
  EEPROM contents
  [0] - Failed attempts on lockscreen
  [1 - KEYBITS/8] - AES key
*/


/*
  Defines
*/
// All user actions. Related to buttons
#define ACTION_UP 0
#define ACTION_DOWN 1
#define ACTION_BACK 2
#define ACTION_ENTER 3

//Unlocking sequence length
#define PASS_LENGTH 1

//Max tries allowed for bad lockscreen sequence
#define MAX_TRIES 3

//Length of the AES key
#define KEYBITS 256

/*
  Globals
*/
//INPUTS contains the pin numbers associated with the touch input buttons
//  Order is UP, DOWN, BACK, ENTER
int INPUTS[] = {16,15,17,18};
//THRESHOLDS contains the threshold value to consider a touch button "pressed"
int THRESHOLDS[] = {510,520,590,730};

//MENU_LINES contains the number of lines to be displayed in a single screen
const int MENU_LINES = 10;
//CURRENT_DIR contains the current directory on the SD card
File CURRENT_DIR;
//CURRENT_POSITION defines the current position in the menu
int CURRENT_POSITION = 0;

//Initializes the LCD display
Teensy3_ST7735 tft = Teensy3_ST7735(10, 9, 8);

//PASSWORD contains the lock screen password
int PASSWORD[PASS_LENGTH] = {0};

//KEY defines the AES key to use
byte KEY[KEYBITS/8] = {0};
//CLEARTEXT is the buffer used to store the unencrypted data
byte CLEARTEXT[65] = {0};
//CRYPTED is the buffer containing the encrypted data
byte CRYPTED[65] = {0};



/*
  serialCommands()
    Manages the management serial communication
  Returns nothing
*/
void serialCommands(void) {
  //Manages the serial communication with the device
  Serial.begin(9600);
  
  while (!Serial) {
    ; // wait for serial port to connect.
  }
  
  Serial.print("\x42\x42");
  Serial.flush();
  
  while (1) {
    //Wait for the first two data bytes
    while (Serial.available() < 2) {
      delay(1);
    }
    
    int command = Serial.read();
    int length = Serial.read();
    
    char data[255] = {0};
    
    for (int i=0; i<length; i++) {
      data[i] = Serial.read();
    }
    
    int path_len = 0;
    char path[256] = {0};
    int field_type = 0;
    int field_len = 0;
    int file_type = 0;
    char field_value[256] = {0};
        
    switch(command) {
      case 1:
        //Create command
        //Extract parameters
        path_len = data[0];
        for (int i=0; i< path_len; i++){
          path[i] = data[i+1];
        }
        file_type = data[path_len+1];
        field_type = data[path_len+2];
        field_len = data[path_len+3];
        if (field_len>64) field_len=64;
        for (int i=0; i< field_len; i++){
          CLEARTEXT[i] = data[i+path_len+4];
        }
        encrypt();
        updateFile(path, file_type, field_type, 64, (char *)CRYPTED);
        break;
      case 3:
        //Sync clock command
        setTime((int)data);
        break;
      case 4:
        listFolder(data);
        break;
      default:
        break;
    }
    
  }
  
  Serial.end();
}

/*
  makeDir()
    Creates a folder on the SD card
    path - The path for the folder
    Sends a \x01 on the serial line if the creation is successfull
    Sends a \x00 if it fails
  Returns nothing
*/
void makeDir(char * path){
  if ( SD.exists(path)) {
    Serial.write('\x00');
  }else{
    SD.mkdir(path);
    Serial.write('\x01');
  }
}

/*
  listFolder()
    Lists the current folder and outputs the result in the serial
    path - A string with the folder name
  A \x01 is sent to the serial line at the end of the procedure.
  Returns nothing
*/
void listFolder(char * path) {
  File folder = SD.open(path);
  if (folder.isDirectory()) {
    while( File content = folder.openNextFile()) {
      if (content.isDirectory()) {
        Serial.print(content.name());
        Serial.println("/");
      }else{
        Serial.println(content.name());
      }
      content.close();
    }
  }else{
    Serial.write('\x00'); //Command NOK, folder is a file
  }
  folder.close();
}

void updateFile(char * path, int file_type, int section_type, int data_len, char * data) {
  int offset = 2;
  File file;
  if (SD.exists(path)) {
    file  = SD.open(path, FILE_WRITE);
  while (file.seek(offset)) {
      int type = file.read();
      if (section_type == type) {
        break;
      }else{
        offset += (file.read()+2); // go to next header offset
      }
    }
  }else{
    file = SD.open(path, FILE_WRITE);
    file.write("\x42");
    file.write((byte)file_type);
  }
  file.seek(offset);
  file.write((byte)section_type);
  file.write((byte)data_len);
  file.write(data);
  file.close();
  Serial.write('\x01');
}

void encrypt (void) {
  AES aes;
  byte iv [16] = {0} ;
  
  byte succ = aes.set_key (KEY, KEYBITS) ;
  //succ = aes.encrypt (CLEARTEXT, CRYPTED) ;
  succ = aes.cbc_encrypt (CLEARTEXT, CRYPTED, 4, iv) ;
}

void decrypt (void) {
  AES aes;
  byte iv [16] = {0} ;
  byte succ = aes.set_key (KEY, KEYBITS) ;
  
  //succ = aes.decrypt (CRYPTED, CLEARTEXT) ;
  succ = aes.cbc_decrypt (CRYPTED, CLEARTEXT, 4, iv) ;
}

/*
  getTOTP()
  Calculates a one time password using TOTP algorithm (RFC 4226)
    seed is the secret seed used to generate the OTP
  Returns a char[6] OTP code
*/
void calculateTOTP(char * secret, int length, char * resultCode) {
  
  time_t time = now()/30;
  
  int data[8] = {0};
  data[4] = (int)((time >> 24) & 0xff);
  data[5] = (int)((time >> 16) & 0xff);
  data[6] = (int)((time >> 8) & 0xff);
  data[7] = (int)(time & 0xff);
  
  Sha1.initHmac((uint8_t *)secret, length);
    
  for (int i=0;i<8;i++){
    Sha1.write((uint8_t) data[i]);
  }
  
  uint8_t * hmac = Sha1.resultHmac();
    
  short offset = hmac[19] & 0x0f;
  int otp = 0;
  for (int i = 0; i<4; i++) {
    otp <<= 8;
    otp  |= hmac[offset+i];
  }
  otp &= 0x7FFFFFFF;
  otp %= 1000000;

  sprintf(resultCode, "%06ld", otp);
  
}


/*
  readButtons()
  Waits for a user to touch a button
  Returns the presesed button code
*/
int readButtons(){
  while (1) {
    for (int i=0;i<4;i++){  //Test each input
      if (touchRead(INPUTS[i]) > THRESHOLDS[i] ){  //Does the input value go over the threshold ?
        delay(300);  // Delay
        return i;  //Return the key code
      }
    }
  }
  delay(10);
}

/*
  nonblock_readButtons()
  Returns a read button (if any) or -1
  Returns the presesed button code
*/
int nonblock_readButtons(){
  for (int i=0;i<4;i++){  //Test each input
    if (touchRead(INPUTS[i]) > THRESHOLDS[i] ){  //Does the input value go over the threshold ?
      delay(300);  // Delay
      return i;  //Return the key code
    }
  }
  return(-1);
}


/*
  drawFolderContents()
  Displays the contents of a folder using pages.
  Size of the page is defined by the MENU_LINES constant
    folder is a File object pointing to a folder
    selected_entry contains the position of the selected entry
  Returns nothing
*/
int drawFolderContents(File folder, int selected_entry){
  //Get number of entries in the folder
  int number_files = 0;
  folder.rewindDirectory();
  while(File tmp = folder.openNextFile()) {
    //We have to close each file as openNextFile() creates a handle
    // to the new file, resulting in a memory leak
    tmp.close();
    number_files++;
  }
  
  folder.rewindDirectory();
  
  //if selected index is out of bounds, return to the first or last one
  if (selected_entry >= number_files) selected_entry = 0;
  if (selected_entry < 0) selected_entry = number_files -1;
  
  //Select the page to display
  unsigned int min_entry = 0;
  unsigned int max_entry = 0;
  if (number_files < MENU_LINES) {
    //Display all folder contents
    min_entry = 0;
    max_entry = number_files;
  } else {
    int page = (selected_entry / MENU_LINES);
    min_entry = page * MENU_LINES;
    max_entry = min_entry + MENU_LINES;
    if (max_entry > number_files) max_entry = number_files;
  }
  
  //list the folder until we get to the entries to display
  for (int i = 0; i < min_entry; i++) {
    File tmp = folder.openNextFile();
    //We have to close each file as openNextFile() creates a handle
    // to the new file, resulting in a memory leak
    tmp.close();
  }
  
  //display the entries
  for (int i = min_entry ; i < max_entry ; i++) {
    File entry = folder.openNextFile();
    if (i == selected_entry) {
      tft.setTextColor(ST7735_BLACK, ST7735_BLUE);
      tft.println(entry.name());
      tft.setTextColor(ST7735_BLUE);
    } else {
      tft.println(entry.name());
    }
    //We have to close each file as openNextFile() creates a handle
    // to the new file, resulting in a memory leak
    entry.close();
  }
  return selected_entry;
}


/*
  getEntry()
    folder is a File object pointing to a folder
    selected_entry contains the position of the selected entry
  Returns a File object containing the selected entry
*/
File getEntry(File folder, int selected_entry) {
  folder.rewindDirectory();
  for (int i = 0; i < selected_entry; i++) {
    File tmp = folder.openNextFile();
    //We have to close each file as openNextFile() creates a handle
    // to the new file, resulting in a memory leak
    tmp.close();
  }
  return folder.openNextFile();
}


/*
  drawHeader()
  Prints the top menu information and title
    text is the text that appears below the title
  Returns nothing
*/
void drawHeader(char * text) {
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_RED);
  tft.println("Pa55ware");
  tft.setTextSize(1);
  tft.setTextColor(ST7735_WHITE);
  tft.println(text);
  tft.setTextColor(ST7735_BLUE);
  tft.println("-----------------");
}

/*
  drawUserPass()
  Displays the user's username and password and waits for user input
    username contains the username
    password - guess what ?
  Returns nothing
*/
void drawUserPass(char * username, char * password) {
  drawHeader("Account details");
  tft.println();
  tft.println("Username :");
  tft.println(username);
  tft.println();
  tft.println("Password :");
  tft.println(password);
  tft.println();
  tft.println("Press Enter to type the password");
  
  while (true) {
    switch(readButtons()){
      case ACTION_UP:
        break;
      case ACTION_DOWN:
        break;
      case ACTION_BACK:
        return;
      case ACTION_ENTER:
        Keyboard.print(password);
        break;
    }
  }
}

/*
  drawOTP()
  Displays the one time password generated in a self-updated screen
   waits for user input
    otp - The OTP to be displayed
  Returns nothing
*/
void drawOTP(char * otp) {
  drawHeader("OTP");
  tft.println();
  tft.println();
  tft.setTextSize(2);
  tft.println(otp);
  tft.setTextSize(1);
  tft.println();
  tft.println();
  tft.println();
  tft.println("Press Enter to type the OTP");
}

/*
  doTOTP()
  Generates the TOTP screen, which is refreshed periodically
    secret - The secret hash to calculate the TOTP
  Returns nothing
*/
void doTOTP(char * secret) {
  int timeValue = 0;
  char otp[6] = {0};
  
  while (true) {
    
    if ((now()/30) != timeValue) {
      //In case the time token has changed
      calculateTOTP((char *)secret, 20, otp);
      drawOTP(otp);
      timeValue = now()/30;
    }
    switch(nonblock_readButtons()){
      case ACTION_UP:
        break;
      case ACTION_DOWN:
        break;
      case ACTION_BACK:
        return;
      case ACTION_ENTER:
        Keyboard.print(otp);
        break;
    }
  }
}

/*
  doFile()
  Performs stuff when a file is selected in the menu
    file is the pointer to the file
  Returns nothing
*/
void doFile(File file) {
  //check for file header
  if (file.read() == 0x42) {
    char username[65] = {0};
    char password[65] = {0};
    switch (file.read()) {
      // User/password file
      case 0x01:
        {
        while(file.available()) {
          int section_type = file.read();
          int section_length = file.read();
          for (int i=0; i<section_length; i++) {
            CRYPTED[i] = file.read();
          }
          decrypt();
          switch(section_type){
            case 0x01:
              for (int i=0; i<64; i++) {
                username[i] = CLEARTEXT[i];
                CLEARTEXT[i] = '\x00';
              }
              break;
            case 0x02:
              for (int i=0; i<64; i++) {
                password[i] = CLEARTEXT[i];
                CLEARTEXT[i] = '\x00';
              }
              break;
          }
        }
        drawUserPass(username, password);
        for(int i=0; i<65; i++) {
          username[i] = '\x00';
          password[i] = '\x00';
        }
        break;
        }
      case 0x02:
        {
        //TOTP file
        char seed[65] = {0};
        while(file.available()) {
          int section_type = file.read();
          int section_length = file.read();
          for (int i=0; i<section_length; i++) {
            CRYPTED[i] = file.read();
          }
          decrypt();
        }
        doTOTP((char*)CLEARTEXT);
        for (int i=0; i<64; i++) {
          CLEARTEXT[i] = '\x00';
        }
        break;
        }
      default:
        return;
    }
  } else {
    return;
  }
}

/*
  lockScreen()
  Displays the lock screen. Can only be bypassed once the correct button sequence has been activated
  Also cleans the AES key from memory before locking the screen.
  AES kes is loaded again after device unlock
  See PASSWORD global to set the password
*/
void lockScreen() {
  int attempts = EEPROM.read(0);
  
  //Clear AES key from memory
  for (int i=0; i<(KEYBITS/8); i++) {
    KEY[i] = '\x00';
  }

  while (attempts < MAX_TRIES) {
    attempts++;
    drawHeader("DEVICE LOCKED");
    tft.println();
    tft.println("Please type your sequence :");
    tft.println();
    int userpass[PASS_LENGTH] = {0};
    for (int i=0; i<PASS_LENGTH; i++) {
      userpass[i] = readButtons();
      tft.setCursor(((i*2)+1)*5, 64);
      tft.print('*');
    }
    //Only do the return at the end to avoid timing attacks
    boolean flag = true;
    for (int i=0; i<PASS_LENGTH; i++) {
      if (userpass[i] != PASSWORD[i]) {
        flag = false;
      } else {
        flag = true;
      }
    }
    if (flag) {
      EEPROM.write(0, 0);
      
      //Once device is unlocked, load the AES key
      for (int i=0; i<(KEYBITS/8); i++) {
        KEY[i] = EEPROM.read(i+1);
      }
      
      return;
    } else {
      EEPROM.write(0, attempts);
      //Delete AES key from EEPROM
      for (int i=0; i<(KEYBITS/8); i++) {
        EEPROM.write(i+1,0);
      }
    }
  }
  drawHeader("DEVICE IS RESET");
  while(true) delay(1000);
}

//Needed to get/set the Teensy RTC
time_t getTeensy3Time()
{
  return Teensy3Clock.get();
}

/*
  SETUP
*/
void setup()  {
  //Init RTC
  setSyncProvider(getTeensy3Time);
  
  //Init LCD
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.setCursor(0, 0);
  tft.fillScreen(ST7735_BLACK);

  //Init SD card
  if (!SD.begin(6)) {
    drawHeader("SD init failed");
    return;
  }
  CURRENT_DIR = SD.open("/");
  
  //Init EEPROM
  if (EEPROM.read(0) == 255 == 255) {
    EEPROM.write(0,0);
  }
  
  //Init is done. If the back button is pressed during bootup, charge the command mode.
  if (nonblock_readButtons() == ACTION_BACK) {
    delay(1000);
    if (nonblock_readButtons() == ACTION_BACK) {
      lockScreen();
      drawHeader("Command mode");
      serialCommands();
      //Serial mode "bypass protection"
      while(true){
        delay(1000);
      }
    }else{
      lockScreen();
    }
  }else{
    lockScreen();
  }
    
}


/*
  MAIN LOOP
*/
void loop() {
  drawHeader(CURRENT_DIR.name());
  CURRENT_POSITION = drawFolderContents(CURRENT_DIR, CURRENT_POSITION);
  switch(readButtons()){
    case ACTION_UP:
      CURRENT_POSITION--;
      break;
    case ACTION_DOWN:
      CURRENT_POSITION++;
      break;
    case ACTION_BACK:
      if (CURRENT_DIR.name() == "/") {
        lockScreen();
      }else{
        CURRENT_DIR = SD.open("/");
      }
      break;
    case ACTION_ENTER:
      File active_entry = getEntry(CURRENT_DIR, CURRENT_POSITION);
      if (active_entry.isDirectory()) {
        CURRENT_DIR.close();
        CURRENT_DIR = active_entry;
      } else {
        doFile(active_entry);
      }
      break;
  }
}

