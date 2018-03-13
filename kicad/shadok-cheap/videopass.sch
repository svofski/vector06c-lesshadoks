EESchema Schematic File Version 4
LIBS:shadok-cache
EELAYER 26 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 9 9
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Text HLabel 2200 1500 0    47   Input ~ 0
SYNCINT
Text HLabel 4000 3050 2    47   Input ~ 0
SYNCp
Text HLabel 4000 1500 2    47   Input ~ 0
SYNCn
Text HLabel 6450 2250 0    47   Input ~ 0
HVSYNC
Wire Wire Line
	3650 3050 4000 3050
Text Label 1800 3050 0    47   ~ 0
V06C_CVBS
Wire Wire Line
	6450 2250 6550 2250
Wire Wire Line
	2300 3050 1800 3050
Wire Wire Line
	9700 4900 10100 4900
$Comp
L power:GND #PWR0916
U 1 1 5C22BB9D
P 10350 4700
F 0 "#PWR0916" H 10350 4450 50  0001 C CNN
F 1 "GND" H 10355 4527 50  0000 C CNN
F 2 "" H 10350 4700 50  0001 C CNN
F 3 "" H 10350 4700 50  0001 C CNN
	1    10350 4700
	1    0    0    -1  
$EndComp
Wire Wire Line
	9700 4700 10350 4700
Wire Wire Line
	9400 4500 9400 4200
Text Label 9400 4200 3    47   ~ 0
PASS_B
Wire Wire Line
	9100 4700 8550 4700
Text Label 8550 4700 0    47   ~ 0
PASS_G
Wire Wire Line
	9100 4900 8550 4900
Text Label 8550 4900 0    47   ~ 0
PASS_R
Wire Wire Line
	6550 2800 6400 2800
Text HLabel 6400 2800 0    47   Input ~ 0
INTENSITY
$Comp
L power:+5V #PWR0915
U 1 1 5C22BC56
P 10300 1400
F 0 "#PWR0915" H 10300 1250 50  0001 C CNN
F 1 "+5V" V 10315 1528 50  0000 L CNN
F 2 "" H 10300 1400 50  0001 C CNN
F 3 "" H 10300 1400 50  0001 C CNN
	1    10300 1400
	0    1    1    0   
$EndComp
Text Label 10100 4900 2    47   ~ 0
VIDEO
$EndSCHEMATC
