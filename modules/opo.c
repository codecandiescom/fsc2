/* 	
 *   (C) 2010 FU Berlin, Dept. of Physics, AG Bittl
 *
 *   Author: Bjoern Beckmann
 *
 *   This file is distributed with fsc2 with permission by
 *   Prof. R. Bittl.
 *
 *   OPTA-OPO module 
 *   SMD-500
 *
 *   Version 1
 *   11/2010
 *   Bugs, questions etc. to bjoernbeckmann(at)hotmail.de
 */

#include "fsc2_module.h"
#include "opo.conf"

#include "serial.h"

/* Define the length of array storing wavelength/motor positions from the BBO_OPO.CAL datafile.
Length depends on (CalLambdaEnd-CalLambdaBegin)/(smallest Stepwidth in opo_calibration)+1 because
the lambdaFromSteps and stepsFromLambda functions need an additional field */
#define CAL_LAMBDA_LENGTH 340

#define SERIAL_BAUDRATE B19200
#define CalBegin 390.
#define CalEnd 730.

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

int opo_serialPortNumber = -1;

float opo_CalLambda[CAL_LAMBDA_LENGTH+1];
int opo_CalMotorPos[CAL_LAMBDA_LENGTH+1];
float opo_CalLambdaBegin;
float opo_CalLambdaEnd;
int opo_CalStepBegin;
int opo_CalStepEnd;
int opo_LengthOfMotorPosArray;

Var_T * opo_name( Var_T * v );
Var_T * opo_initializeDevice( Var_T * v );
Var_T * opo_goTo( Var_T * v );
Var_T * opo_scan( Var_T * v );
Var_T * opo_setStepperFrequency(Var_T * v);
Var_T * opo_originSearch(Var_T * v);
Var_T * opo_status(Var_T * v);
Var_T * opo_stop(Var_T * v);
Var_T * opo_idlerFromSignal(Var_T * v);
Var_T * opo_signalFromIdler(Var_T * v);
Var_T * opo_calibration(Var_T * v);
Var_T * opo_setOffset(Var_T * v);
Var_T * opo_goToInteractive(Var_T * v);

void opo_sendCommand(char* c);
void opo_setAcceleration(int StepperStartFrequenz,int StepperMaxFrequenz,int StepperAccLength);
void opo_stepperSetAbsPosition(int ParAbsolutePosition);
void opo_stepperSetAbsVarFreq(int ParAbsolutePosition,int GoToSpeed);
void opo_autoPromptEnable(void);
void opo_autoPromptDisable(void);
void opo_triggerCount(int events);
void opo_specialWait(void);
void opo_promptOnTriggerEnable(void);
void opo_promptOnTriggerDisable(void);
void opo_deceleratingStop(void);
void opo_requestPrompt(void);
int opo_stepsFromLambda(float ParLambda);
float opo_lambdaFromSteps(int steps);
float opo_signalFromIdlerIntern(float idler);
float opo_idlerFromSignalIntern(float opo_signal);
int opo_reachedPosition(int stepPosition,int b);
void opo_failure(const char* info );
void opo_checkCorrectRange(int motorStatus,int stepPositionErg,int b);

int opo_init_hook(       void );
int opo_test_hook(       void );
int opo_end_of_test_hook( void );
int opo_exp_hook(        void );
int opo_end_of_exp_hook( void );
void opo_exit_hook(      void );

/* EDL FUNCTIONS */

Var_T * opo_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}

Var_T * opo_initializeDevice( Var_T * v  UNUSED_ARG )
{
	char cmd_str[10];
	cmd_str[0] = (char) 0x1;
	cmd_str[1] = (char) 0;
	cmd_str[2] = (char) 0;
	cmd_str[3] = (char) 0;
	cmd_str[4] = (char) 0;
	cmd_str[5] = (char) 0;
	cmd_str[6] = (char) 0;
	cmd_str[7] = (char) 0;
	cmd_str[8] = (char) 0;
	cmd_str[9] = (char) 0;

	opo_sendCommand(cmd_str);
	fsc2_usleep(1000000,UNSET);
	opo_setAcceleration(3000,12000,4000);
	fsc2_usleep(1000000,UNSET);

	Var_T * func_ptr;
	func_ptr = func_get("opo_originSearch", NULL);
	func_call(func_ptr);		
	
	return vars_push( INT_VAR, 1 );

}

/* opo_goTo expects two parameters:
	float goToPos as Signal (between min/max-value from BBO_OPO.cal)
	float goToSpeed (between 0.1 - 20.)
*/
Var_T * opo_goTo( Var_T * v ){

	float goToPos=get_double(v,"float");
	float goToSpeed=get_double(v->next,"float");

	if(goToPos<opo_CalLambdaBegin || goToPos>opo_CalLambdaEnd){
		print( FATAL, "Wavelength out of range: %E - %E \n",opo_CalLambdaBegin,opo_CalLambdaEnd );
    		THROW( EXCEPTION );
	}else if(goToSpeed<0.1 || goToSpeed>20.)
		opo_failure("GoToSpeed out of range: 0.1 - 20.");
	else{
		int st = opo_stepsFromLambda(goToPos);		
		opo_stepperSetAbsVarFreq(st,round(400*goToSpeed));
		opo_reachedPosition(st,1);
	}

	return vars_push( INT_VAR, 1 );

}

/* opo_scan expects 6 parameters:
	float scanStart as Signal (between min/max-value from BBO_OPO.cal)
	float scanStop as Signal (between min/max-value from BBO_OPO.cal)
	float scanSpeed (between 0.1 - 20.)
	bool scanMode, 0 or "off" for continuous mode, 1 or "on" for triggered mode 
	int scanEvents (between 1 and 10)
	float increment (between 1. and 10.)
*/

Var_T * opo_scan( Var_T * v ){

	Var_T * c;
	float scanStart=get_double(v,"float");
	c=v->next;
	float scanStop=get_double(c,"float");
	c=c->next;
	float scanSpeed=get_double(c,"float");
	c=c->next;
	bool scanMode=get_boolean(c);
	c=c->next;
	vars_check(c,INT_VAR);
	int scanEvents=c->INT;
	c=c->next;
	float increment=get_double(c,"float");

	if(scanStart<opo_CalLambdaBegin || scanStart>opo_CalLambdaEnd){
		print( FATAL, "scanStart out of range: %E - %E \n",opo_CalLambdaBegin,opo_CalLambdaEnd );
    		THROW( EXCEPTION );
	}else if(scanStop<opo_CalLambdaBegin || scanStop>opo_CalLambdaEnd){
		print( FATAL, "scanStop out of range: %E - %E \n",opo_CalLambdaBegin,opo_CalLambdaEnd );
    		THROW( EXCEPTION );
	}else if(scanSpeed<0.1 || scanSpeed>20.)
		opo_failure("scanSpeed out of range: 0.1 - 20.");
	else if(scanEvents<1 || scanEvents>10)
		opo_failure("scanEvents out of range: 1-10");
	else if(increment<1. || increment>10.)
		opo_failure("increment out of range: 1.-10.");

	opo_setAcceleration(3000,12000,1000);
	fsc2_usleep(100000,UNSET);

	if(FSC2_MODE==EXPERIMENT) print(NO_ERROR, "opo moves to the scan start point.\n");

	int start = opo_stepsFromLambda(scanStart);
	opo_stepperSetAbsPosition(start);
	opo_reachedPosition(start,1);

	if(FSC2_MODE==EXPERIMENT) print(NO_ERROR, "opo reached the scan start point.\n");

	if(!scanMode){

		if(FSC2_MODE==EXPERIMENT) print(NO_ERROR, "opo starts continued scan.\n");

		int st = opo_stepsFromLambda(scanStop);		
		opo_stepperSetAbsVarFreq(st,round(140*scanSpeed));
		opo_reachedPosition(st,1);

		if(FSC2_MODE==EXPERIMENT) print(NO_ERROR, "opo reached the scan end point.\n");

	}else{ //triggered mode
	opo_autoPromptDisable();		
	fsc2_usleep(100000,UNSET);
	opo_triggerCount(scanEvents);
	fsc2_usleep(100000,UNSET);
	opo_promptOnTriggerEnable();
	fsc2_usleep(100000,UNSET);
	opo_specialWait();
	fsc2_usleep(100000,UNSET);

	char cmd[14];	
	int motorStatus,motorPos1,motorPos2,motorPos3,pos,position,steps;
	float lambda;
	ssize_t lenCheck;

	if(FSC2_MODE==EXPERIMENT){

	// "leeren" der seriellen Schnittstelle
	if( (lenCheck = fsc2_serial_read(opo_serialPortNumber,cmd,sizeof cmd,"",-1,SET)) != sizeof cmd){
			if(lenCheck==0) opo_failure("(leeren) No data could be read from opo");
			else{
				opo_failure("Can't read message from opo");
			}
	}


	print(NO_ERROR, "opo starts triggered scan.\n");
	int etap;	

	do{
		if( (lenCheck = fsc2_serial_read(opo_serialPortNumber,cmd,sizeof cmd,"",-1,SET)) != sizeof cmd){
			if(lenCheck==0) opo_failure("No data could be read from opo");
			else{
				opo_failure("Can't read message from opo");
			}
		}

		if(cmd[0]=='[' && cmd[13]==']'){
			motorStatus=(unsigned char)cmd[3]&31;

			if(motorStatus==2){

				motorPos1=(unsigned char)cmd[4];
				motorPos2=(unsigned char)cmd[5];
				motorPos3=(unsigned char)cmd[6];
				pos=((motorPos2<<8)|motorPos1);
				position=motorPos3;
				position=((position<<16)|pos);
				steps=position;			

				opo_checkCorrectRange(motorStatus,steps,1);
				lambda=opo_lambdaFromSteps(steps);
			
				if(scanStart<=scanStop) lambda=lambda+increment;
				else lambda=lambda-increment;			

				if(lambda<opo_CalLambdaBegin || lambda>opo_CalLambdaEnd){
					print( FATAL, "opo is out of calibrated range: %E - %E \n",opo_CalLambdaBegin,opo_CalLambdaEnd );
			    		THROW( EXCEPTION );
				}

				etap=opo_stepsFromLambda(lambda);
				opo_stepperSetAbsPosition(etap);
				fsc2_usleep(2000000,UNSET); // don't use opo_reachedPosition(etap), because autoPrompt is disabled at this state			
				print(NO_ERROR, "current position(Signal): %E nm \n",lambda);

				if (    (    scanStart <= scanStop
						  && lambda + increment <= scanStop )
					 || (    scanStart > scanStop
						  && lambda - increment >= scanStop ) ){
					opo_specialWait();
				}else{
					opo_promptOnTriggerDisable();
					fsc2_usleep(100000,UNSET);
					opo_autoPromptEnable();
					fsc2_usleep(100000,UNSET);

					print(NO_ERROR, "opo reached the scan end point.\n");

					break;
				}
			
			}//motorStatus==2
	}
	
	}while(true);
	
	}//EXPMODE
	
	}//else(!scanMode)		

	return vars_push( INT_VAR, 1 );
}

/* opo_setStepperFrequency expects one parameter:
	int stepperMaxFrequency (between 200 and 6000)

-sets default stepper speed

*/
Var_T * opo_setStepperFrequency(Var_T * v){

	vars_check(v,INT_VAR);
	int stepperMaxFrequency = v->INT;
	if(stepperMaxFrequency<200 || stepperMaxFrequency>6000){
		opo_failure("stepperMaxFrequency out of range: 200 - 6000");
	}else{
		int stepperStartFrequency,stepperAccLength;
		stepperStartFrequency=round(stepperMaxFrequency/2);
  		stepperAccLength=round(stepperMaxFrequency*2/3);
    		opo_setAcceleration(stepperStartFrequency,stepperMaxFrequency,stepperAccLength);
		fsc2_usleep(100000,UNSET);
	}

	return vars_push(INT_VAR, 1);
}

Var_T * opo_originSearch(Var_T * v  UNUSED_ARG ){

	opo_autoPromptDisable();
	fsc2_usleep(100000,UNSET);

	char cmd_str[10];
	cmd_str[0] = (char) 0x8;
	cmd_str[1] = (char) 1;
	cmd_str[2] = (char) 2;
	cmd_str[3] = (char) 1;
	cmd_str[4] = (char) 0;
	cmd_str[5] = (char) 0;
	cmd_str[6] = (char) 0;
	cmd_str[7] = (char) 0;
	cmd_str[8] = (char) 0;
	cmd_str[9] = (char) 0;
	opo_sendCommand(cmd_str);
	fsc2_usleep(100000,UNSET);

	char cmd[14];	
	int motorStatus,motorPos1,motorPos2,motorPos3,pos,position,steps;
	ssize_t lenCheck;

	if(FSC2_MODE==EXPERIMENT){

	do{
		if( (lenCheck = fsc2_serial_read(opo_serialPortNumber,cmd,sizeof cmd,"",-1,SET)) != sizeof cmd){
			if(lenCheck==0) opo_failure("No data could be read from opo");
			else{
				opo_failure("Can't read message from opo");
			}
		}

		if(cmd[0]=='[' && cmd[13]==']'){
			motorStatus=(unsigned char)cmd[3]&31;
			motorPos1=(unsigned char)cmd[4];
			motorPos2=(unsigned char)cmd[5];
			motorPos3=(unsigned char)cmd[6];
			pos=((motorPos2<<8)|motorPos1);
			position=motorPos3;
			position=((position<<16)|pos);
			steps=position;			

			print(NO_ERROR, "current step position: %d , motor status: %d , error: %d , status controller: %d\n",steps, motorStatus,(unsigned char) cmd[1]&15, (unsigned char) cmd[2]&0x3);

			if((motorStatus&4)==4){
				opo_autoPromptEnable();
				if((steps)==0){

					int st = opo_stepsFromLambda(420.);		
					opo_stepperSetAbsVarFreq(st,4000);
					opo_reachedPosition(st,2);
					print(NO_ERROR, "Origin Search finished. Current position(Signal): 420 nm \n");

					return vars_push(INT_VAR, 1);
				}
			}
		}

		fsc2_usleep(1000000,UNSET);

	}while(true);
	
	}//EXPERIMENT	

	return vars_push(INT_VAR, 1);
}

Var_T * opo_status(Var_T * v  UNUSED_ARG ){

	char cmd[14];	

	int motorStatus,errorB,contrStatus,motorPos1,motorPos2,motorPos3,pos,position;

	ssize_t lenCheck;

	if(FSC2_MODE==EXPERIMENT){

		if( (lenCheck = fsc2_serial_read(opo_serialPortNumber,cmd,sizeof cmd,"",5000000,SET)) != sizeof cmd){
			if(lenCheck==0) opo_failure("No data could be read from opo");
			else{
				opo_failure("Can't read message from opo");
			}
		}

		if(cmd[0]=='[' && cmd[13]==']'){
			
			errorB=(unsigned char)cmd[1]&15;
			contrStatus=(unsigned char)cmd[2]&0x3;
			motorStatus=(unsigned char)cmd[3]&31;
			motorPos1=(unsigned char)cmd[4];
			motorPos2=(unsigned char)cmd[5];
			motorPos3=(unsigned char)cmd[6];
			pos=((motorPos2<<8)|motorPos1);
			position=motorPos3;
			position=((position<<16)|pos);

			opo_checkCorrectRange(motorStatus,position,1);

			print(NO_ERROR, "current position(Signal): %E nm, stepper position: %d , motor status: %d , error: %d , contrl.status: %d \n",opo_lambdaFromSteps(position),position,motorStatus,errorB,contrStatus);

		}
	}

	return vars_push(INT_VAR, 1);

}

Var_T * opo_stop(Var_T * v UNUSED_ARG ){

	opo_deceleratingStop();
	fsc2_usleep(100000,UNSET);
	opo_promptOnTriggerDisable();
	fsc2_usleep(100000,UNSET);
	opo_autoPromptEnable();
	fsc2_usleep(100000,UNSET);

	return vars_push(INT_VAR, 1);

}

// Parameter: float, signal in nm
Var_T * opo_idlerFromSignal(Var_T * v){

	float opo_signal=get_double(v,"float");
	if(opo_signal<opo_CalLambdaBegin || opo_signal>opo_CalLambdaEnd){
		print( FATAL, "parameter out of range: %E - %E \n",opo_CalLambdaBegin,opo_CalLambdaEnd );
    		THROW( EXCEPTION );
	}
	return vars_push(FLOAT_VAR, (1./(1./354.767-1./opo_signal)));

}

// Parameter: float, idler in nm
Var_T * opo_signalFromIdler(Var_T * v){

	float opo_idler=get_double(v,"float");
	if(opo_idler>opo_idlerFromSignalIntern(opo_CalLambdaBegin) || opo_idler<opo_idlerFromSignalIntern(opo_CalLambdaEnd)){
		print( FATAL, "parameter out of range: %E - %E \n",opo_idlerFromSignalIntern(opo_CalLambdaEnd),opo_idlerFromSignalIntern(opo_CalLambdaBegin) );
    		THROW( EXCEPTION );
	}

	return vars_push(FLOAT_VAR, (1./(1./354.767-1./opo_idler)));

}

/*
	float begin; start point in nm (must be >= CalBegin)
	float end; end point in nm (must be <= CalEnd)
	float step; difference between two points in nm (range 1. - 50.)
	
	user input: 	"ok" -> calibrated position is ok; calibrate next step position
			integer num -> opo moves to "current step position" + num (f.e. -100, f.e. 20)

	care about write permissions of BBO_OPO.cal, u.c. u should start fsc2 with root rights
*/

Var_T * opo_calibration(Var_T * v){

	Var_T * c;
	float begin=get_double(v,"float");
	c=v->next;
	float end=get_double(c,"float");
	c=c->next;
	float step=get_double(c,"float");

	if(begin<CalBegin || end>CalEnd){
		print(FATAL, "Input Error: Begin Calibration Position < %E  or End Calibration Position > %E",CalBegin,CalEnd);
    		THROW( EXCEPTION );
	}else if(begin>420. || end<420.){
		opo_failure("Input Error: Begin Calibration Position > 420 or End Calibration Position < 420.");
	}else if(begin >= end){
		opo_failure("Input Error: Begin Calibration > End Calibration");
	}else if(step<1. || step>50.){
		opo_failure("Input Error: stepwidth < 1.0 or stepwidth > 50.0");
	}

	if(FSC2_MODE==EXPERIMENT){

	FILE *datei;
	datei=fopen(BBO_OPO_PATH_OLD,"r");
	if(datei!=NULL){
		if(remove(BBO_OPO_PATH_OLD)!=0) opo_failure("Cant remove BBO_OPO.old");	
	}

	if(rename(BBO_OPO_PATH,BBO_OPO_PATH_OLD)!=0) opo_failure("Can't rename BBO_OPO.cal");

	print(NO_ERROR, "Opo moves to the position entered\n");
	
	float realBegin=begin;
	if(begin<opo_CalLambdaBegin) realBegin=opo_CalLambdaBegin;

	Var_T * func_ptr;
	func_ptr = func_get("opo_goTo", NULL);
	vars_push(FLOAT_VAR,realBegin);
	vars_push(FLOAT_VAR,10.);
	func_call(func_ptr);		

	print(NO_ERROR, "Opo reached the specified position\n");
	int g = 0;
	float actualLambda=begin;
	int actualStepPosition = opo_stepsFromLambda(realBegin);	

	print(NO_ERROR, "current stepper position: %d\n",actualStepPosition);

	char stringE[50];
	int eingabe;

	while(true){

		printf("Enter additional stepper steps:");		
		if(scanf("%s",stringE)==1){
			if(stringE[0]=='o' && stringE[1] == 'k'){
				g++;
				datei = fopen(BBO_OPO_PATH,"a");
				if(datei==NULL) opo_failure("Can't open BBO_OPO.cal");
				fprintf(datei,"%d %E %d\n",g,actualLambda,actualStepPosition);
				fclose(datei);
				print(NO_ERROR,"saved stepper position %d to wavelength %E\n",actualStepPosition,actualLambda);
				actualLambda+=step;
				if(actualLambda>end) break;
				print(NO_ERROR,"current calibration wavelength: %E , current stepper position: %d\n",actualLambda, actualStepPosition);

			}else{

				eingabe=atoi(stringE);
				if(eingabe==0){ 
					printf("Incorrect input: just enter an integer or enter \"ok\" .");				
				}else{
					if(actualStepPosition+eingabe<0){
						print(NO_ERROR, "Error. Current stepper position would be < 0. \n");
						continue;
					}
					actualStepPosition+=eingabe;
					opo_stepperSetAbsVarFreq(actualStepPosition,4000);
					opo_reachedPosition(actualStepPosition,0);
					print(NO_ERROR, "current stepper position: %d\n",actualStepPosition);
				}
			}
		}				

	}	
	
	}//Experiment

	return vars_push(INT_VAR, 1);

}

/*
	expects 1 float variable: opo position in nm as signal

	care about write permissions of BBO_OPO.cal, u.c. u should start fsc2 with root rights
*/

Var_T * opo_setOffset(Var_T * v){

	float nm=get_double(v,"float");

	if(nm<opo_CalLambdaBegin || nm>opo_CalLambdaEnd){
		print( FATAL, "Wavelength out of range: %E - %E \n",opo_CalLambdaBegin,opo_CalLambdaEnd );
    		THROW( EXCEPTION );
	}

	if(FSC2_MODE==EXPERIMENT){

	print(NO_ERROR, "Opo moves to the position entered\n");

	Var_T * func_ptr;
	func_ptr = func_get("opo_goTo", NULL);
	vars_push(FLOAT_VAR,nm);
	vars_push(FLOAT_VAR,10.);
	func_call(func_ptr);		

	print(NO_ERROR, "Opo reached the specified position\n");

	int startStepPosition = opo_stepsFromLambda(nm);	

	print(NO_ERROR, "current stepper position: %d\n",startStepPosition);

	char stringE[50];
	int eingabe;
	int os;
	int actualStepPosition=startStepPosition;	

	while(true){

		printf("Enter additional stepper steps:");		
		if(scanf("%s",stringE)==1){
			if(stringE[0]=='o' && stringE[1] == 'k'){
				os = actualStepPosition-startStepPosition;
				FILE *datei;
				datei = fopen(BBO_OPO_PATH,"w");
				if(datei==NULL) opo_failure("Can't open BBO_OPO.cal");
				int g=1;
				while(g<=opo_LengthOfMotorPosArray){
					fprintf(datei,"%d %E %d\n",g,opo_CalLambda[g],(opo_CalMotorPos[g]+os));
					g++;
				}
				fclose(datei);

				print(NO_ERROR,"saved offset: %d\n",os);
				break;

			}else{

				eingabe=atoi(stringE);
				if(eingabe==0){ 
					printf("Incorrect input: just enter an integer or enter \"ok\" .\n");				
				}else{
					if(actualStepPosition+eingabe<0){
						print(NO_ERROR, "Error. Current stepper position would be < 0. \n");
						continue;
					}
					actualStepPosition+=eingabe;
					opo_stepperSetAbsVarFreq(actualStepPosition,4000);
					opo_reachedPosition(actualStepPosition,0);
					print(NO_ERROR, "current stepper position: %d\n",actualStepPosition);
				}
			}

		}			

	}//while	

	}//Experiment
	return vars_push(INT_VAR, 1);

}


/*
	expects 1 float variable: opo position in nm as signal
	user input: 	"end" -> experiment will finish
			float num -> opo moves to position "num"

*/
Var_T * opo_goToInteractive(Var_T * v){

	float nm=get_double(v,"float");

	if(nm<opo_CalLambdaBegin || nm>opo_CalLambdaEnd){
		print( FATAL, "Wavelength out of range: %E - %E \n",opo_CalLambdaBegin,opo_CalLambdaEnd );
    		THROW( EXCEPTION );
	}

	if(FSC2_MODE==EXPERIMENT){

	print(NO_ERROR, "Opo moves to the position entered\n");

	Var_T * func_ptr;
	func_ptr = func_get("opo_goTo", NULL);
	vars_push(FLOAT_VAR,nm);
	vars_push(FLOAT_VAR,10.);
	func_call(func_ptr);		

	print(NO_ERROR, "Opo reached the specified position.The interactive mode starts. \n");

	char stringE[50];
	float eingabe;

	while(true){

		printf("Enter wavelength:");		
		if(scanf("%s",stringE)==1){
			if(stringE[0]=='e' && stringE[1] == 'n' && stringE[2] == 'd'){
				break;
			}else{
				eingabe=atof(stringE);
				if(eingabe==0){ 
					printf("Incorrect input: just enter a float or enter \"end\" . \n");				
				}else{
					Var_T * func_ptr2;
					func_ptr2 = func_get("opo_goTo", NULL);
					vars_push(FLOAT_VAR,eingabe);
					vars_push(FLOAT_VAR,10.);
					func_call(func_ptr2);		
					print(NO_ERROR, "Opo reached the specified position.\n");
				}
			}

		}			

	}//while	

	}//Experiment
	return vars_push(INT_VAR, 1);

}

/* NON-EDL HELP FUNCTIONS */

void opo_sendCommand(char* c)
{	
	char cmd[13];	
	cmd[0] = (char) 0x3c;
	unsigned char checkSum = 0;
	int i;
	for(i=0; i<11; i++){
		if(i<10) cmd[i+1] = c[i];
		checkSum += cmd[i];
	}
	cmd[11] = checkSum;
	cmd[12] = (char) 0x3e;
	
	ssize_t lenCheck;

	if(FSC2_MODE==EXPERIMENT){
		if( (lenCheck = fsc2_serial_write(opo_serialPortNumber,cmd,sizeof cmd,100000,UNSET)) !=sizeof cmd){
			if(lenCheck==0) opo_failure("No data could be written to opo");
			else opo_failure("Can't write command to opo");

		}

//	printf("%i \n",errno);

	}	

}

void opo_setAcceleration(int StepperStartFrequenz,int StepperMaxFrequenz,int StepperAccLength){

	char cmd_str[10];
	cmd_str[0] = (char) 0x2;
	cmd_str[1] = (char) (StepperStartFrequenz & 0xFF);
	cmd_str[2] = (char) ((StepperStartFrequenz>>8) & 0xFF);
	cmd_str[3] = (char) ((StepperStartFrequenz>>16) & 0xFF);
	cmd_str[4] = (char) (StepperMaxFrequenz & 0xFF);
	cmd_str[5] = (char) ((StepperMaxFrequenz>>8) & 0xFF);
	cmd_str[6] = (char) ((StepperMaxFrequenz>>16) & 0xFF);
	cmd_str[7] = (char) (StepperAccLength & 0xFF);
	cmd_str[8] = (char) ((StepperAccLength>>8) & 0xFF);
	cmd_str[9] = (char) ((StepperAccLength>>16) & 0xFF);
	opo_sendCommand(cmd_str);
}

void opo_stepperSetAbsPosition(int ParAbsolutePosition){

	char cmd_str[10];
	cmd_str[0] = (char) 0x7; //move absolute
	cmd_str[1] = (char) 1; //motor#1
	cmd_str[2] = (char) (ParAbsolutePosition & 0xFF);
	cmd_str[3] = (char) ((ParAbsolutePosition>>8) & 0xFF);
	cmd_str[4] = (char) ((ParAbsolutePosition>>16) & 0xFF);
	cmd_str[5] = (char) ((ParAbsolutePosition>>24) & 0xFF);
	cmd_str[6] = (char) 0;
	cmd_str[7] = (char) 0;
	cmd_str[8] = (char) 0;
	cmd_str[9] = (char) 0;
	opo_sendCommand(cmd_str);
}

void opo_stepperSetAbsVarFreq(int ParAbsolutePosition, int GoToSpeed){

	char cmd_str[10];
	cmd_str[0] = (char) 0x18; // constant speed absolute move, variable frequency
	cmd_str[1] = (char) 1; //motor#1
	cmd_str[2] = (char) 1; //start immediately
	cmd_str[3] = (char) (ParAbsolutePosition & 0xFF);
	cmd_str[4] = (char) ((ParAbsolutePosition>>8) & 0xFF);
	cmd_str[5] = (char) ((ParAbsolutePosition>>16) & 0xFF);
	cmd_str[6] = (char) ((ParAbsolutePosition>>24) & 0xFF);
	cmd_str[7] = (char) (GoToSpeed & 0xFF);
	cmd_str[8] = (char) ((GoToSpeed>>8) & 0xFF);
	cmd_str[9] = (char) ((GoToSpeed>>16) & 0xFF);
	opo_sendCommand(cmd_str);
}

void opo_autoPromptEnable(void){

	char cmd_str[10];
	cmd_str[0] = (char) 0xf;
	cmd_str[1] = (char) 1;
	cmd_str[2] = (char) 1;
	cmd_str[3] = (char) 1;
	cmd_str[4] = (char) 1;
	cmd_str[5] = (char) 1;
	cmd_str[6] = (char) 1;
	cmd_str[7] = (char) 1;
	cmd_str[8] = (char) 1;
	cmd_str[9] = (char) 1;
	opo_sendCommand(cmd_str);
}

void opo_autoPromptDisable(void){

	char cmd_str[10];
	cmd_str[0] = (char) 0x10;
	cmd_str[1] = (char) 0;
	cmd_str[2] = (char) 0;
	cmd_str[3] = (char) 0;
	cmd_str[4] = (char) 0;
	cmd_str[5] = (char) 0;
	cmd_str[6] = (char) 0;
	cmd_str[7] = (char) 0;
	cmd_str[8] = (char) 0;
	cmd_str[9] = (char) 0;
	opo_sendCommand(cmd_str);
}

void opo_triggerCount(int events){

	char cmd_str[10];
	cmd_str[0] = (char) 0x11;
	cmd_str[1] = (char) (events & 0xFF);
	cmd_str[2] = (char) ((events>>8) & 0xFF);
	cmd_str[3] = (char) ((events>>16) & 0xFF);
	cmd_str[4] = (char) ((events>>24) & 0xFF);
	cmd_str[5] = (char) 0;
	cmd_str[6] = (char) 0;
	cmd_str[7] = (char) 0;
	cmd_str[8] = (char) 0;
	cmd_str[9] = (char) 0;
	opo_sendCommand(cmd_str);
}

void opo_specialWait(void){

	char cmd_str[10];
	cmd_str[0] = (char) 0x14;
	cmd_str[1] = (char) 0;
	cmd_str[2] = (char) 0;
	cmd_str[3] = (char) 0;
	cmd_str[4] = (char) 0;
	cmd_str[5] = (char) 0;
	cmd_str[6] = (char) 0;
	cmd_str[7] = (char) 0;
	cmd_str[8] = (char) 0;
	cmd_str[9] = (char) 0;
	opo_sendCommand(cmd_str);
}

void opo_promptOnTriggerEnable(void){

	char cmd_str[10];
	cmd_str[0] = (char) 0x0d;
	cmd_str[1] = (char) 0;
	cmd_str[2] = (char) 0;
	cmd_str[3] = (char) 0;
	cmd_str[4] = (char) 0;
	cmd_str[5] = (char) 0;
	cmd_str[6] = (char) 0;
	cmd_str[7] = (char) 0;
	cmd_str[8] = (char) 0;
	cmd_str[9] = (char) 0;
	opo_sendCommand(cmd_str);
}

void opo_promptOnTriggerDisable(void){

	char cmd_str[10];
	cmd_str[0] = (char) 0x0e;
	cmd_str[1] = (char) 0;
	cmd_str[2] = (char) 0;
	cmd_str[3] = (char) 0;
	cmd_str[4] = (char) 0;
	cmd_str[5] = (char) 0;
	cmd_str[6] = (char) 0;
	cmd_str[7] = (char) 0;
	cmd_str[8] = (char) 0;
	cmd_str[9] = (char) 0;
	opo_sendCommand(cmd_str);
}

void opo_deceleratingStop(void){

	char cmd_str[10];
	cmd_str[0] = (char) 0x4;
	cmd_str[1] = (char) 0;
	cmd_str[2] = (char) 0;
	cmd_str[3] = (char) 0;
	cmd_str[4] = (char) 0;
	cmd_str[5] = (char) 0;
	cmd_str[6] = (char) 0;
	cmd_str[7] = (char) 0;
	cmd_str[8] = (char) 0;
	cmd_str[9] = (char) 0;
	opo_sendCommand(cmd_str);

}

void opo_requestPrompt(void){

	char cmd_str[10];
	cmd_str[0] = (char) 0x17;
	cmd_str[1] = (char) 0;
	cmd_str[2] = (char) 0;
	cmd_str[3] = (char) 0;
	cmd_str[4] = (char) 0;
	cmd_str[5] = (char) 0;
	cmd_str[6] = (char) 0;
	cmd_str[7] = (char) 0;
	cmd_str[8] = (char) 0;
	cmd_str[9] = (char) 0;
	opo_sendCommand(cmd_str);

}

/* opo_stepsFromLambda expects ParLambda as Signal
Then it computes the step motor position for a given wavelength based on the BBO_OPO.CAL datafile
-opo_stepsFromLambda pre condition: parLambda is in calibrated range !
*/
int opo_stepsFromLambda(float ParLambda){
	int i,MotorPosDiff;
	float LambdaDiff;
	i=0;
	do{
		i++;
		LambdaDiff=ParLambda-opo_CalLambda[i];
		if(opo_CalLambda[i]==opo_CalLambdaEnd){
			opo_CalLambda[i+1]=opo_CalLambda[i]+(opo_CalLambda[i]-opo_CalLambda[i-1]);
			opo_CalMotorPos[i+1]=opo_CalMotorPos[i]+(opo_CalMotorPos[i]-opo_CalMotorPos[i-1]);
		}

	}while(LambdaDiff>=((opo_CalLambda[i+1]-opo_CalLambda[i])/2));

	MotorPosDiff=opo_CalMotorPos[i+1]-opo_CalMotorPos[i];

	int sol = opo_CalMotorPos[i]+round((MotorPosDiff/(opo_CalLambda[i+1]-opo_CalLambda[i]))*LambdaDiff);

	return sol;

}

/* opo_lambdaFromSteps returns wavelength as Signal
-opo_lambdaFromSteps pre condition: "steps" is in calibrated range !
*/

float opo_lambdaFromSteps(int steps){
	int i;
	float DiffMotPos,LambdaDiff,lambda;

		i=0;
		do{
			i++;
			DiffMotPos = steps-opo_CalMotorPos[i];
			if(opo_CalMotorPos[i]==opo_CalStepEnd){
				opo_CalLambda[i+1]=opo_CalLambda[i]+(opo_CalLambda[i]-opo_CalLambda[i-1]);
				opo_CalMotorPos[i+1]=opo_CalMotorPos[i]+(opo_CalMotorPos[i]-opo_CalMotorPos[i-1]);
			}
		}while(DiffMotPos>=(opo_CalMotorPos[i+1]-opo_CalMotorPos[i])/2);
		LambdaDiff=opo_CalLambda[i+1]-opo_CalLambda[i];
		lambda=opo_CalLambda[i]+((DiffMotPos*LambdaDiff)/(opo_CalMotorPos[i+1]-opo_CalMotorPos[i]));

		return lambda;
}

float opo_signalFromIdlerIntern(float idler){
	return 1./(1./354.767-1./idler);
}

float opo_idlerFromSignalIntern(float opo_signal){
	return 1./(1./354.767-1./opo_signal);
}

// b==0, in case of calibration or setting offset
// b==2, in case of origin search
// b==1, else

int opo_reachedPosition(int stepPosition,int b){

	char cmd[14];	
	int motorStatus,motorPos1,motorPos2,motorPos3,pos,position;
	int stepPositionErg=-1;
	float lambda;
	ssize_t lenCheck;

	if(FSC2_MODE==EXPERIMENT){
	
	time_t t1,t2;
	time(&t1);

	do{	

		if( (lenCheck = fsc2_serial_read(opo_serialPortNumber,cmd,sizeof cmd,"",5000000,SET)) != sizeof cmd){
			if(lenCheck==0) opo_failure("No data could be read from opo");
			else{
				opo_failure("Can't read message from opo");
			}
		}

		if(cmd[0]=='[' && cmd[13]==']'){
			motorStatus=(unsigned char)cmd[3]&31;
			motorPos1=(unsigned char)cmd[4];
			motorPos2=(unsigned char)cmd[5];
			motorPos3=(unsigned char)cmd[6];
			pos=((motorPos2<<8)|motorPos1);
			position=motorPos3;
			position=((position<<16)|pos);
			stepPositionErg=position;

			opo_checkCorrectRange(motorStatus,stepPositionErg,b);

			if(b==1){//no calibration, no setting offset,no origin search
				lambda = opo_lambdaFromSteps(stepPositionErg);		
	
				time(&t2);

				if( difftime(t2,t1)>=4. || stepPosition==stepPositionErg ){
					print(NO_ERROR, "current position: %E nm \n",lambda);
					time(&t1);
				}
				
			}
		}

	}while(stepPosition!=stepPositionErg);

	}//EXPERIMENT	
	
	return stepPositionErg;
}

void opo_failure(const char* info ) {
    print( FATAL, "%s \n", info );
    THROW( EXCEPTION );
}

// b==0, in case of calibration,setting offset
// b==2, in case of origin search
// b==1, else


void opo_checkCorrectRange(int motorStatus,int stepPositionErg, int b){

			
			if(b==0 || b==1){//no origin search
				if((motorStatus&4)==4){
					print(NO_ERROR,"Opo calibration end stop is pressed and the Opo moves to the start point from BBO_OPO.cal\n");
					opo_deceleratingStop();
					fsc2_usleep(1000000,UNSET);
					opo_stepperSetAbsVarFreq(opo_CalStepBegin,4000);
					opo_reachedPosition(opo_CalStepBegin,2);
					opo_failure("Opo calibration end stop was pressed and the Opo moved up to the start point from BBO_OPO.cal\n");
				}
				if((motorStatus&8)==8){
					print(NO_ERROR,"opo reached the lower limit switch and moves up to the start point from BBO_OPO.cal\n");
					opo_deceleratingStop();
					fsc2_usleep(1000000,UNSET);
					opo_stepperSetAbsVarFreq(opo_CalStepBegin,4000);
					opo_reachedPosition(opo_CalStepBegin,2);
					opo_failure("opo reached the lower limit switch and moved up to the start point from BBO_OPO.cal\n");
				}
				if((motorStatus&16)==16){
					print(NO_ERROR,"opo reached the upper limit switch and moves up to the end point from BBO_OPO.cal\n");
					opo_deceleratingStop();
					fsc2_usleep(1000000,UNSET);
					opo_stepperSetAbsVarFreq(opo_CalStepEnd,4000);
					opo_reachedPosition(opo_CalStepEnd,2);
					opo_failure("opo reached the upper limit switch and moved up to the end point from BBO_OPO.cal\n");
				}

			}			
			if(b==1){//no calibration, no setting offset, no origin search
				if(stepPositionErg<opo_CalStepBegin ){
					print(NO_ERROR,"opo is under the calibrated range and moves up to the start point from BBO_OPO.cal\n");
					opo_deceleratingStop();
					fsc2_usleep(1000000,UNSET);
					opo_stepperSetAbsVarFreq(opo_CalStepBegin,4000);
					opo_reachedPosition(opo_CalStepBegin,2);
					opo_failure("opo was under the calibrated range and moved up to the start point from BBO_OPO.cal\n");
				}else if(stepPositionErg>opo_CalStepEnd){
					print(NO_ERROR,"opo is over the calibrated range and moves up to the end point from BBO_OPO.cal\n");
					opo_deceleratingStop();
					fsc2_usleep(1000000,UNSET);
					opo_stepperSetAbsVarFreq(opo_CalStepEnd,4000);
					opo_reachedPosition(opo_CalStepEnd,2);
					opo_failure("opo was over the calibrated range and moved up to end point from BBO_OPO.cal\n");
				}			
			}
}

/*            Hook functions                 */

/*------------------------------------------------------*
 * Function called when the module has just been loaded
 *------------------------------------------------------*/

int opo_init_hook( void )
{
	FILE *datei;
	datei = fopen(BBO_OPO_PATH,"r");
	int err,g;
	g=0;
	if(datei==NULL) opo_failure("Can't open BBO_OPO.cal");
	else{
		do{
			g++;
			err = fscanf(datei,"%i %E %i",&opo_LengthOfMotorPosArray,&opo_CalLambda[g],&opo_CalMotorPos[g]);
		}while(err!=EOF);
	}
	fclose(datei);
	opo_CalLambdaBegin=opo_CalLambda[1];
	opo_CalLambdaEnd=opo_CalLambda[g-1];
	opo_CalStepBegin=opo_CalMotorPos[1];
	opo_CalStepEnd=opo_CalMotorPos[g-1];

	opo_serialPortNumber = fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );
	
    return 1;
}


/*----------------------------------------------*
 * Function called at the start of the test run
 *----------------------------------------------*/

int opo_test_hook( void )
{
    return 1;
}

/*----------------------------------------------*
 * Function called at the end of the test run
 *----------------------------------------------*/

int opo_end_of_test_hook( void )
{
   return 1;
}

/*-----------------------------------------------*
 * Function called at the start of an experiment
 *-----------------------------------------------*/

int opo_exp_hook( void )
{	

	/*
		serial communication parameters: 19200 Baud, no handshake, 8 data bits, 1 stop bit, no parity	
	*/

	struct termios * tio;
	if( ( tio = fsc2_serial_open(opo_serialPortNumber, O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL)
		opo_failure("Can't access opo");

//	printf("%i\n",errno);

	tio->c_cflag = 0; 
	tio->c_cflag |= CS8;
    	tio->c_cflag |= CLOCAL | CREAD;

	tio->c_iflag = IGNBRK;
	tio->c_oflag = 0;
	tio->c_lflag = 0;

	cfsetispeed(tio, SERIAL_BAUDRATE );
	cfsetospeed(tio, SERIAL_BAUDRATE );

	fsc2_tcflush(opo_serialPortNumber, TCIOFLUSH );
	fsc2_tcsetattr(opo_serialPortNumber, TCSANOW, tio );

	opo_autoPromptEnable();
	fsc2_usleep(100000,UNSET);
	opo_setAcceleration(3000,6000,4000);
	fsc2_usleep(100000,UNSET);

    return 1;
}


/*---------------------------------------------*
 * Function called at the end of an experiment
 *---------------------------------------------*/

int opo_end_of_exp_hook( void )
{
	fsc2_serial_close(opo_serialPortNumber);
    return 1;
}


/*------------------------------------------------------*
 * Function called just before the module gets unloaded
 *------------------------------------------------------*/

void opo_exit_hook( void )
{
}
