#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// Teams
class Team {
  public:
    String name;
    int score = 0;
    int suitToSink = 7; // excludes the 8-ball
    int scorelessTurns = 2;  // decreases by one every time team goes a turn without sinking at least one of their own balls. Delay of game penalty when it reaches zero.
    Team(String n) {this->name = n;}
};
Team home = Team("HME");  // Team variables are defined globally so they can be used by updateBoard() function
Team away = Team("AWY");
Team *playing;    // points to the team that is currently playing
Team *notPlaying; // should be pretty obvious..

// game state variables
int state = 10; // state of the internal state machine
int rack = 1;
bool suits = false;
int maxScorelessTurns = 2;
bool fouled = false;
bool bonus = false;

// score input variables
int own = 0;
int opp = 0;
int key = -1;

// keypad setup
const byte ROWS = 4; //four rows
const byte COLS = 4; //three columns
char keys[ROWS][COLS] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};
byte rowPins[ROWS] = {9,8,7,6}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {5,4,3,2}; //connect to the column pinouts of the keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

// lcd setup
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 column and 2 rows
byte solChar[] = { B01110, B11111, B11111, B11111, B11111, B11111, B11111, B01110 };
byte stpChar[] = { B01110, B10001, B10001, B11111, B11111, B10001, B10001, B01110 };
byte noSinkTwo[] = { B11111, B11111, B11111, B00000, B00000, B11111, B11111, B11111 };
byte noSinkOne[] = { B00000, B00000, B00000, B00000, B00000, B11111, B11111, B11111 };

void updateBoard() {
  lcd.clear();
  
  lcd.setCursor(0,0); lcd.print(away.name);
  lcd.setCursor(4,0); lcd.print(away.score);
  
  lcd.setCursor(7,0);
  if(playing == &home) {lcd.print("->");}
  else if (playing == &away) {lcd.print("<-");}

  lcd.setCursor(10,0); lcd.print(home.score);
  lcd.setCursor(13,0); lcd.print(home.name);

  // suits is true => home is on solids; away is on stripes
  if(suits == true) {
    lcd.setCursor(2,1); lcd.write(0);
    lcd.setCursor(13,1); lcd.write(1);
  }
  else {
    lcd.setCursor(2,1); lcd.write(1);
    lcd.setCursor(13,1); lcd.write(0);
  }

  lcd.setCursor(0,1); lcd.print(away.suitToSink);
  lcd.setCursor(7,1); lcd.print(rack);
  lcd.setCursor(15,1); lcd.print(home.suitToSink);

  lcd.setCursor(4,1);
  switch(away.scorelessTurns) {
    case 2: lcd.write(3); break;
    case 1: lcd.write(4); break;
    default: lcd.print(""); break;
  }

  lcd.setCursor(10,1);
  switch(home.scorelessTurns) {
    case 2: lcd.write(3); break;
    case 1: lcd.write(4); break;
    default: lcd.print(""); break;
  }

  // bonus turn indicator
  if(bonus) {
    if(playing == &home) { lcd.setCursor(14,1); }
    else if(playing == &away) { lcd.setCursor(1,1); }
    lcd.print("*");
  }
  else {
    lcd.setCursor(14,1); lcd.print("");
    lcd.setCursor(1,1); lcd.print("");
  }
}

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, solChar);
  lcd.createChar(1, stpChar);
  lcd.createChar(3, noSinkTwo);
  lcd.createChar(4, noSinkOne);

  updateBoard();
}

void loop() {

  switch(state) {
    case 10:  // pre-rack setup
      // determine who gets to play this rack (home gets even racks)
      if(rack % 2 == 0) { playing = &home; notPlaying = &away; }
      else { playing = &away; notPlaying = &home; }
      
      state = 20;
      updateBoard();  // to get the arrow in the correct orientation
      break;

    case 20:  // getting "own" from keypad
      key = keypad.getKey();
      if(key >= 48 && key <= 57){
        own = key-48;
        state = 21;
      }
      else if(key == 68){ // D is the "oops i messed up" key
        own = 0;
        opp = 0;
        fouled = false;
        state = 20;
      }
      else if(key == 65) {
        suits = !suits;
        updateBoard();
      }
      break;

    case 21: // setting "opp"
      key = keypad.getKey();
      if(key >= 48 && key <= 57){
        opp = key-48;
        state = 22;
      }
      else if(key == 68){ // D is the "oops i messed up" key
        own = 0;
        opp = 0;
        fouled = false;
        state = 20;
      }
      else if(key == 65) {
        suits = !suits;
        updateBoard();
      }
      break;

    case 22:  // waiting for "foul" indication: 0 => no foul; yes => foul
      key = keypad.getKey();
      if(key == 48) {   // 0
        state = 30; 
      }
      else if(key == 49) {  // 1
        fouled = true;
        state = 30;
      }
      else if(key == 68){ // D is the "oops i messed up" key
        own = 0;
        opp = 0;
        fouled = false;
        state = 20;
      }
      else if(key == 65) {
        suits = !suits;
        updateBoard();
      }
      break;

    case 30:  // processing scoring information
      // update counts of how many balls each team has left to sink (until getting to the 8 ball)
      if(own > 0) { // if at least one ball was sunk
        playing->suitToSink -= own;
        playing->scorelessTurns = maxScorelessTurns;  // reset scoreless turns counter
      }
      else {  // no balls were sunk
        playing->scorelessTurns -= 1; // this will just keep decrementing, even past 0, until they sink a ball
        if(playing->scorelessTurns <= 0) { fouled = true; }
      }
      if(opp > 0) { notPlaying->suitToSink -= opp; fouled = true; }

      if(playing->suitToSink < 0 || notPlaying->suitToSink < 0) { state = 50; } // if either team has sunk all their balls, rack is over
      else { state = 40; }
      break;

    case 40:  // turn over; choosing which team goes next
      
      // switch playing team
      if(playing == &home) { playing = &away; notPlaying = &home; }
      else { playing = &home; notPlaying = &away; }

      if(fouled == true) {
        fouled = false;
        bonus = true; // set up for bonus turn indication
      }
      else { bonus = false; } // if no foul was made, do not indicate bonus turn

      own = 0; opp = 0; // reset scorekeeping variables

      state = 20;
      updateBoard();
      break;

    case 50:  // end-of-rack state
      if(playing->suitToSink <= 0 && notPlaying->suitToSink <= 0) { playing->score += 3; }  // both teams "down to the 8-ball" => winning team gets three points
      else { playing->score += notPlaying->suitToSink; }  // otherwise, winning team gets points based on how many balls the opponent left on the table

      playing->suitToSink = 7;
      notPlaying->suitToSink = 7;

      playing->scorelessTurns = maxScorelessTurns;
      notPlaying->scorelessTurns = maxScorelessTurns;
      
      own = 0; opp = 0; fouled = false; bonus = false; // reset scorekeeping variables
      state = 10;
      rack++;
      updateBoard();
      break;
  }
    
}