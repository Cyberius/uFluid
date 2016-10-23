#define SIN_LEN_QUARTER 8
#define SIN_LEN         (SIN_LEN_QUARTER*4)
#define CLOCK_FREQ      16000000
#define SINE_DIVISOR    8
#define SINE_INT_OVF    17

#define MAX_FREQ        100
#define MAX_AMPL        100
#define MAX_RATE        100

#define FILTER_ALPHA    9
#define MASK_MAX        3
#define WINDUP_MAX      10

#define P1_PIN          0
#define SOL1_PIN        9
#define SOL2_PIN        10

char sineArray[SIN_LEN_QUARTER + 1];

volatile int timer = 0;
volatile int overflow = 100;
volatile int go = 0;
char freq;
char ampl = MAX_AMPL;
char fsweep_freq_start;
char fsweep_freq_end;
int fsweep_rate;
volatile int fsweep_count;
char fsweeping = 0;

void setup()
{
  Serial.begin(9600);
  pinMode( SOL1_PIN, OUTPUT );
  pinMode( SOL2_PIN, OUTPUT );
  
} 

void loop()
{
  typedef enum
  {
    MODE_NONE = 0,
    MODE_INVALID,
    MODE_MASK,
    MODE_AMPL,
    MODE_P,
    MODE_I,
    MODE_D,
  } E_COMMS_MODE;
  
  char outMask = 0;
  int target = 0;
  float drive = 0;
  int value;
  E_COMMS_MODE comms_mode = MODE_NONE;
  float error;
  float last_error = 0;
  float p = 2;
  float i = 0.01;
  float d = 30.0;
  float integral = 0;
  float pressure = 0;
  bool control = 1;
  float pressure_calib;
  
  while ( 1 )
  {
    while ( Serial.available() > 0 )
    {
      int val_int;
      float val_float;
      char mode_char;
      
      switch ( comms_mode )
      {
        case MODE_NONE:
          mode_char = Serial.read(); 
          
          switch ( mode_char )
          {
            case 'M':
            case 'm':
              comms_mode = MODE_MASK;
              break;
              
            case 'A':
            case 'a':
              comms_mode = MODE_AMPL;
              break;
              
            case 'P':
            case 'p':
              comms_mode = MODE_P;
              break;
              
            case 'I':
            case 'i':
              comms_mode = MODE_I;
              break;
              
            case 'D':
            case 'd':
              comms_mode = MODE_D;
              break;
              
            default:
              comms_mode = MODE_INVALID;
              break;
          }
          break;
          
        case MODE_P:
        case MODE_I:
        case MODE_D:
          val_float = Serial.parseFloat();
          break;
        case MODE_MASK:
        case MODE_AMPL:
          val_int = Serial.parseInt();
          break;
        default:
          break;
      }
      
      if ( Serial.peek() == '\n' )
      {
        Serial.read();
        
        switch ( comms_mode )
        {
          case MODE_MASK:
            outMask = (byte)val_int;
            control = 0;
            break;
            
          case MODE_AMPL:
            target = (int)val_int;
            control = 1;
            break;
            
          case MODE_P:
            p = val_float;
            break;
            
          case MODE_I:
            i = val_float;
            break;
            
          case MODE_D:
            d = val_float;
            break;
            
          default:
            Serial.print( "Invalid" );
            break;
        }
        
        Serial.print( "\r\n" );
        comms_mode = MODE_NONE;
      }
    }
    
    digitalWrite( SOL1_PIN, ( outMask & 1 ) ? HIGH : LOW );
    digitalWrite( SOL2_PIN, ( outMask & 2 ) ? HIGH : LOW );
    
    value = analogRead( P1_PIN );
    pressure = ( FILTER_ALPHA * pressure + (float)value ) / 10;
    error = ( (float)target - pressure_calib ) * ( 230.0/60.0 );
    integral += error * i;
    if ( integral < -(float)WINDUP_MAX )
      integral = -(float)WINDUP_MAX;
    else if ( integral > (float)WINDUP_MAX )
      integral = (float)WINDUP_MAX;
    drive = error * p + integral + ( error - last_error ) * d;
    if ( drive < 0.0 )
      drive = 0.0;
    else if ( drive > (float)MASK_MAX )
      drive = (float)MASK_MAX;
    
    if ( control )
      outMask = round(drive);
    last_error = error;
    
    pressure_calib = ( pressure * 5.0 / 1024.0 - 0.5 ) * 100;
    
    Serial.print( "(" );
    Serial.print( (int)target );
    Serial.print( " / " );
    Serial.print( (float)pressure_calib );
    Serial.print( ") / (" );
    Serial.print( (int)value );
    Serial.print( " / " );
    Serial.print( (float)pressure );
    Serial.print( ") / " );
    Serial.print( (float)drive );
    Serial.print( " / (" );
    Serial.print( (float)p );
    Serial.print( " / " );
    Serial.print( (float)i );
    Serial.print( " / " );
    Serial.print( (float)d );
    Serial.print( " / " );
    Serial.print( (float)integral );
    Serial.print( ")" );
    Serial.println( "" );
  }
} 

