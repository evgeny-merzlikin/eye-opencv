#include "GLFWMain.h"

extern LPVM_MIDI_PORT MIDIport;

void sendMidiNote(unsigned char channel, unsigned char pitch, unsigned char volume) {

	if ( !MIDIport )
		return;

	unsigned char message [] = {channel, pitch, volume};
	if ( !virtualMIDISendData(MIDIport, message, 3) )
		printf("virtualMIDISendData error: %d\n", GetLastError());

}

void sendMidiNoteOn(unsigned char channel, unsigned char pitch, unsigned char volume) {
	sendMidiNote(0x90 & channel, pitch, volume);
}

void createMIDIPort()
{
	virtualMIDILogging( TE_VM_LOGGING_MISC | TE_VM_LOGGING_RX | TE_VM_LOGGING_TX );

	MIDIport = virtualMIDICreatePortEx2( PORTNAME, NULL, NULL, MAX_SYSEX_BUFFER, TE_VM_FLAGS_PARSE_RX );

	if ( !MIDIport )
		printf("virtualMIDICreatePortEx2 error: %d\n", GetLastError());
}

void destroyMIDIPort() 
{
	if ( MIDIport ) 
		virtualMIDIClosePort( MIDIport );
}