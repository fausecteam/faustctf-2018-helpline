#!/bin/bash

export GOOGLE_APPLICATION_CREDENTIALS=./access_token.json

function g_tts {
	curl -H "Authorization: Bearer "$(gcloud auth application-default print-access-token) -H "Content-Type: application/json; charset=utf-8" --data "{
	  'input':{
	    'text':\"$2\"
	  },
	  'voice':{
	    'languageCode':'en-us',
	    'name':'en-US-Wavenet-F',
	    'ssmlGender':'FEMALE'
	  },
	  'audioConfig':{
	    'audioEncoding':'LINEAR16'
	  }
	}" "https://texttospeech.googleapis.com/v1beta1/text:synthesize" | grep audioContent | awk -F\" {'print $4'} | base64 -d > $1
}

function e_tts {
	echo "$2" | espeak -w $1
}

function f_tts {
	echo "$2" | text2wave > $1
}

function gen {
#	f_tts tmp.wav "$2"
#	e_tts tmp.wav "$2"
	g_tts tmp.wav "$2"
	sox tmp.wav -t raw -r 8000 -c 1 -e mu-law -b 8 IVR/$1.mulaw
}

#gen welcome "Welcome to our crypto currencies helpline! If you feel compelled to buy new crypto currencies every day and cannot afford a warm meal any more, please press 1."
#gen hodl "Don't worry, we are here to help! How many cryptocurrencies do you currently hodl? If you hodl more than nine cryptocurrencies, please press 9."
#gen buy_instead "We are glad that you are not addicted to buying cryptocurrencies. But while you are at it, may we offer you our latest shitcoin, Astral Ledger. Our special offer: 100 Astral Ledger tokens for just 9001 Dollars. Press 1 to confirm and 2 to cancel your purchase."
#gen notmuch_buy "Dou you really own less than three cryptocurrencies? You obviously are not addicted and need to expand your portfolio. May we offer you our latest shitcoin, Astral Ledger? Our special offer: 100 Astral Ledger tokens for just 9001 Dollars. Press 1 to confirm and 2 to cancel your purchase."
#gen alreadyknown "That phone number is already known to us. Press 1 if you would like to be connected to one of our support agents again, 2 if you would like to hear your credit card number and 3 if you would like to change your credit card number."
#gen error "We are sorry, an error occured. Please call back again later."
#gen phone_1 "Holy shit. That is a lot of cryptocurrencies. But we are here to offer professional help. Please enter your phone number and confirm using the hash key."
#gen phone_2 "We are delighted to sell you our newest shitcoin. You will soon be contacted by our authorized sales agent. Please enter your phone number and confirm using the hash key."
#gen bye_1 "Thank you. You will be called back soon. Goodbye."
#gen billed "Thank you. Please also enter your credit card number and confirm using the hash key."
#gen callback "Thanks again. Would you like to be called back or stay connected and wait for one of our agents to become available? Press 1 for a callback and 2 to wait for your agent."
#gen bye_2 "We are very disappointed. Goodbye."
#gen important "Your call is very important to us. Please stand by, you will soon be connected to one of our support agents."
#gen sup_init "Welcome to our call center agent interface. Press 1 to retrieve the current user counter. Press 2 to retrieve the personal information for a specific user ID."
#gen sup_data "Please enter the numeric user ID of the user to retrieve data from and confirm using the hash key."
#gen update "Thank you. Please enter your new credit card number and confirm using the hash key."
