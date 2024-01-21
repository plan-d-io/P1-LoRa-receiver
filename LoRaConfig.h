/*SF, BW and  wait times used to sync transmitter and receiver
 Needs to be indentical on both transmitter and receiver*/
static const byte loraConfig[][4] ={
  {12, 125, 8, 38},
  {12, 250, 3, 20},
  {11, 250, 2, 10},
  {10, 250, 2, 8},
  {9, 250, 2, 8},
  {8, 250, 2, 8},
  {7, 250, 1, 8}
};

/*ms between updates to keep in line with 1% LoRa duty cycle, for single and three phase meter telegrams
 Needs to be indentical on both transmitter and receiver*/
static const unsigned long loraUpdate[][7] ={
  {230474, 107207, 57803, 31099, 16700, 9000, 4500},
  {372346, 173169, 93369, 50238, 26977, 14538, 7000}
  };

/*Template of meter telegram*/
float meterData[] = {
  999999.999, //totConT1
  999999.999, //totConT2
  999999.999, //totInT1
  999999.999, //totInT2
  99.999,     //TotpowCon
  99.999,     //TotpowIn
  999999.999, //avgDem
  999999.999, //maxDemM
  999.99,     //volt1
  999.99,     //current1
  999999.999, //totGasCon
  999999.999, //totWatCon
  99.999,     //powCon1
  999999.999, //powCon2
  999999.999, //powCon3
  99.999,     //powIn1
  999999.999, //powIn2
  999999.999,  //powIn3
  999.99,     //volt2
  999.99,     //volt3
  999.99,     //current2
  999.99,     //current3
  0,          //pad
  0           //pad
};
