/* Filter coefficient for smoothing pressure readings */
#define FILTER_ALPHA    9

/* Maximum output value, dependent on number of solenoids
  2 solenoids = 2 bits = 0b11 = 3
*/
#define MASK_MAX        3

/* Used to limit integrator windup */
#define WINDUP_MAX      10

/* Pin locations */
#define P1_PIN          0
#define SOL1_PIN        9
#define SOL2_PIN        10

void setup()
{
  /* Initialize serial comms */
  Serial.begin(9600);
  
  /* Set solenoid pins as outputs */
  pinMode( SOL1_PIN, OUTPUT );
  pinMode( SOL2_PIN, OUTPUT );
} 

void loop()
{
  /* Commands */
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
  
  E_COMMS_MODE comms_mode = MODE_NONE;
  char outMask = 0;         // Solenoid output bit mask
  int target = 0;           // Control target
  float drive = 0;          // Control output
  int value;                // Raw pressure reading
  float error;              // Control error
  float last_error = 0;     // Control error from last iteration
  float p = 2;              // Control Proportional coefficient
  float i = 0.01;           // Control Integral coefficient
  float d = 30.0;           // Control Differential coefficient
  float integral = 0;       // Control integral accumulator  
  float pressure = 0;       // Filtered pressure in ADC counts
  float pressure_calib;     // Filtered pressure in kPa
  bool control = 1;         // Control loop enabled
  
  while ( 1 )
  {
    /* Process characters in comms buffer */
    while ( Serial.available() > 0 )
    {
      int val_int;
      float val_float;
      char mode_char;
      
      switch ( comms_mode )
      {
        case MODE_NONE:
          /* Decide which command we have received */
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
        
        /* Read additional values, depending on command */
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
      
      /* The command is finished when we get an end of line character */
      if ( Serial.peek() == '\n' )
      {
        Serial.read();
        
        /* Process command and associated parameters */
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
    
    /* Read pressure */
    value = analogRead( P1_PIN );
    
    /* Apply filter to smooth pressure reading */
    pressure = ( FILTER_ALPHA * pressure + (float)value ) / 10;
    
    /* Apply calibration */
    pressure_calib = ( pressure * 5.0 / 1024.0 - 0.5 ) * 100;
    
    /* Control loop start.  Calculate error. */
    error = ( (float)target - pressure_calib ) * ( 230.0/60.0 );
    
    /* Calculate integral and apply windup limiting. */
    integral += error * i;
    if ( integral < -(float)WINDUP_MAX )
      integral = -(float)WINDUP_MAX;
    else if ( integral > (float)WINDUP_MAX )
      integral = (float)WINDUP_MAX;
    
    /* Calculate output, consisting of P, I and D factors. */
    drive = error * p + integral + ( error - last_error ) * d;
    
    /* Limit output to be within range allowed by number of solenoids */
    if ( drive < 0.0 )
      drive = 0.0;
    else if ( drive > (float)MASK_MAX )
      drive = (float)MASK_MAX;
    
    /* If we have set a control target, translate control output to
       solenoid output mask.
    */
    if ( control )
      outMask = round(drive);
    
    /* Record last value for use in next iteration */
    last_error = error;
    
    /* Set solenoid outputs */
    digitalWrite( SOL1_PIN, ( outMask & 1 ) ? HIGH : LOW );
    digitalWrite( SOL2_PIN, ( outMask & 2 ) ? HIGH : LOW );
    
    /* Print control data */
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

