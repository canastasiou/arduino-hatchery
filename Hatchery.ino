#include "Wire.h"
#include <dht.h>
#include <DateTime.h>
#include <DateTimeStrings.h>
#include <LiquidCrystal.h>
#include <Servo.h>

//Pins for sensor that measure temperature and humidity
#define DHT_PIN1 A4
#define DHT_PIN2 A5
//Pin for servo that moves the eggs
#define SERVO_PIN 3
//The lamp is turned on to raise the temperature.
#define LAMP_SWITCH A0
//The humidifier is turned on to raise the humidity of the hatchery.
#define HUMIDIFIER_SWITCH A1
//The fan is turned on whenever the lamp or humidifier are on, to help spread the effect around the hatchery
#define FAN_SWITCH A2
//How many measurements are stored and used for calculations
#define MEASUREDVALUES 10

//LCD
LiquidCrystal lcd(4, 5, 6, 7, 8, 9);
//Egg shaker. Gently moves the eggs in specific intervals.
Servo eggMover;
//Current position of the egg mover.
int eggMoverPositionPosition;
//Current time
unsigned long time;
//The next interval at which the eggs will be moved.
unsigned long eggNextMoveInterval;
//The interval at which the eggs were last moved.
unsigned long eggLastMoveInterval;
//Initialize the array with the temperature measurements. The average of these values will be calculated and used.
double temperatureValues[MEASUREDVALUES] = {20,20,20,20,20,20,20,20,20,20};
//Initialize the array with the humidity measurements. The average of these values will be calculated and used.
double humidityValues[MEASUREDVALUES] = {20,20,20,20,20,20,20,20,20,20};
int measurementIndex = 0;
dht DHTSensor1;
dht DHTSensor2;

void setup() {
	Serial.begin(19200);
	measurementIndex = 0;

	//Calibrate Lamp, Humidifier and Fan
	pinMode(LAMP_SWITCH, OUTPUT);
	pinMode(HUMIDIFIER_SWITCH, OUTPUT);
	pinMode(FAN_SWITCH, OUTPUT);

	eggNextMoveInterval = 28800000;
	eggLastMoveInterval = 0;

	//Start the LCD
	lcd.begin(16, 4);

	//Calibrate the egg mover servo
	eggMover.attach(SERVO_PIN);
	eggMoverPositionPosition = 0;
	eggMover.write(eggMoverPositionPosition);

	delay(700);
}

void loop() 
{
	time = millis();
	double temperature1;
	double humidity1;
	double temperature2;
	double humidity2;
	double temperatureAverage = 0;
	double humidityAverage = 0;
	
	int days, hours, minutes, seconds, tempResult;

	//Get temperatures and humidities
	DHTSensor1.read11(DHT_PIN1); //get humidity, temperature
	temperature1 = DHTSensor1.temperature;
	humidity1 = DHTSensor1.humidity;
	Serial.println("Temp1: ");
	Serial.print(temperature1);
	Serial.println("Hum1: ");
	Serial.print(humidity1);
	DHTSensor2.read11(DHT_PIN2); //get humidity, temperature
	temperature2 = DHTSensor2.temperature;
	humidity2 = DHTSensor2.humidity;
	Serial.print("Temp1: ");
	Serial.println(temperature2);
	Serial.print("Hum1: ");
	Serial.println(humidity2);
	Serial.println("-------------------------");

	//We store the average of the measurements received by the two sensors
	temperatureValues[measurementIndex] = (temperature1 + temperature2) / 2;
	humidityValues[measurementIndex] = (humidity1 + humidity2) / 2;

	measurementIndex++;
	if(measurementIndex == MEASUREDVALUES) measurementIndex = 0;

	Serial.println("Array values.");
	for(int count = 0; count < MEASUREDVALUES; count++){
		Serial.print("Position: ");
		Serial.println(count);
		Serial.print(" Temp Avg: ");
		Serial.println(temperatureValues[count]);
		Serial.print(" Humidity Avg: ");
		Serial.println(humidityValues[count]);
		temperatureAverage = temperatureAverage + temperatureValues[count];
		humidityAverage = humidityAverage + humidityValues[count];
	}
	Serial.println("-------------------------");

	//Calculate the average of the stored measurements
	temperatureAverage = temperatureAverage / MEASUREDVALUES;
	humidityAverage = humidityAverage / MEASUREDVALUES;
	//Check measurements and turn fan, humidifier and lamp on/off.
	checkMeasurements(temperatureAverage, humidityAverage, time);

	// Calculate days, hours, minutes, seconds to show on LCD
	tempResult = time / 1000; //From milliseconds to seconds
	seconds = tempResult % 60;
	tempResult = tempResult / 60; //From seconds to minutes
	minutes = tempResult % 60;
	tempResult = tempResult / 60; //From minutes to hours
	hours = tempResult % 24;
	days = tempResult / 24; //From hours to days
	
	lcd.setCursor(0, 0);
	lcd.print(days);
	lcd.print(":");
	lcd.print(hours);
	lcd.print(":");
	lcd.print(minutes);
	lcd.print(":");
	lcd.print(seconds);
	lcd.setCursor(0, 1);
	lcd.print("Temp: ");
	lcd.print(temperatureAverage);
	lcd.print("    ");
	lcd.setCursor(0, 2);
	lcd.print("Hum: ");
	lcd.print(humidityAverage);
	lcd.print("    ");

	delay(10000);
}

void checkMeasurements(double temperature, double humidity, unsigned long time){
	double temperatureThreshold;
	double humidityThreshold;
	//Times when each stage ends
	unsigned long stage1EndTime = 1641600000; //18+1 days
	unsigned long stage2EndTime = 1900800000; //21+1 days
	boolean lampOn = false;
	boolean humidifierOn = false;
	boolean fanOn = false;
	boolean motorOn = false;

	//Set thresholds according to stage
	if(time < stage1EndTime){
		temperatureThreshold = 38;
		humidityThreshold = 50;
	} else if(time < stage2EndTime){
		temperatureThreshold = 38;
		humidityThreshold = 65;
	} else{
		temperatureThreshold = 38;
		humidityThreshold = 65;
	}

	//Temperature check. If under the treshold we want
	//to increase it by turning the lamp and the fan on
	if(temperature < temperatureThreshold){
		lampOn = true;
		fanOn = true;
	}

	//Humidity check. If under the threshold we want
	//to increase it by turning the humidifier and the lamp on.
	if(humidity < humidityThreshold){
		humidifierOn = true;
		fanOn = true;
	}

	//Calculate whether to move the eggs
	if(time >= eggNextMoveInterval && time < stage2EndTime){
		motorOn = true;
		eggLastMoveInterval = eggNextMoveInterval;
		eggNextMoveInterval = eggNextMoveInterval + 28800000;
		Serial.println("Started Moving eggs.");
		Serial.println(eggMoverPositionPosition);
		//Slowly move the egg mover servo towards the opposite direction.
		if(eggMoverPositionPosition == 0){
			for(; eggMoverPositionPosition < 180; eggMoverPositionPosition += 1)
			{
				Serial.println(eggMoverPositionPosition);
				eggMover.write(eggMoverPositionPosition);
				delay(30);
			} 
		}
		else{
			for(; eggMoverPositionPosition > 0; eggMoverPositionPosition -= 1)
			{
				Serial.println(eggMoverPositionPosition);
				eggMover.write(eggMoverPositionPosition);
				delay(30);
			}
		}
		Serial.println("Finished Moving eggs.");
	}

	//Send the actual signals to turn devices on/off
	if(lampOn){
		digitalWrite(LAMP_SWITCH, LOW);
	}else{
		digitalWrite(LAMP_SWITCH, HIGH);
	}

	if(humidifierOn){
		digitalWrite(HUMIDIFIER_SWITCH, LOW);
	}else{
		digitalWrite(HUMIDIFIER_SWITCH, HIGH);
	}

	if(fanOn){
		digitalWrite(FAN_SWITCH, LOW);
	}else{
		digitalWrite(FAN_SWITCH, HIGH);
	}

	// Print lamp, fan, humidifier and motor indicators on lcd
	lcd.setCursor(0, 3);
	if(lampOn) lcd.print("LAM ");
	else lcd.print("    "); 
	if(fanOn) lcd.print("FAN ");
	else lcd.print("    "); ;
	if(humidifierOn) lcd.print("HUM ");
	else lcd.print("    "); 
	if(motorOn) lcd.print("MOT");
	else lcd.print("   ");
}
