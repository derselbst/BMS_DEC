#include <stdio.h>
#include <stdint.h>

// not sure how many track there can be?
#define TRACKS 255

unsigned char notes[8];

// holds the current track we are handling
unsigned char tracknum=0;

// on which channel we are currently operating
uint8_t current_channel=0;

// number of ticks passed since the last event
int delay=0;

// number of ticks passed since the last event, only used in the main track (i.e. the
// track containing the tempo and metadata events)
int basedelay=0;

// contains size of each track in bytes
unsigned int tracksz[TRACKS]= {0};

// holds the offset where to find the next track in the bms file
int savepos=0;

// are we in the main track?
int inmain=1;

uint16_t ppqn=0;
uint16_t tempo=0;

enum branch
{
    BR_NORMAL,
    BR_C1,
    BR_FF
};

enum ctrl_type
{
    VOLUME,
    PAN
};

unsigned char midi_status_note_on(unsigned char chan=current_channel)
{
    // only lower nibble for channel specification
    chan &= 0b00001111;

    chan |= 0b1001 << 4;

    return chan;
}

unsigned char midi_status_note_off(unsigned char chan=current_channel)
{
    // only lower nibble for channel specification
    chan &= 0b00001111;

    chan |= 0b1000 << 4;

    return chan;
}

unsigned char midi_status_pitch_wheel(unsigned char chan=current_channel)
{
    // only lower nibble for channel specification
    chan &= 0b00001111;

    chan |= 0b1110 << 4;

    return chan;
}

unsigned char midi_status_prog_change(unsigned char chan=current_channel)
{
    // only lower nibble for channel specification
    chan &= 0b00001111;

    chan |= 0b1100 << 4;

    return chan;
}

unsigned char midi_status_control_change(unsigned char chan=current_channel)
{
    // only lower nibble for channel specification
    chan &= 0b00001111;

    chan |= 0b1011 << 4;

    return chan;
}

unsigned long long to_var_len(unsigned long long value)
{
    unsigned long long buffer;

    buffer = value & 0x7f;

    while((value >>= 7)>0)
    {
        buffer<<= 8;
        buffer |= 0x80;
        buffer += (value&0x7f);
    }

    return buffer;
}

void write_var_len(unsigned long long value, FILE* out)
{
    unsigned long long buffer = to_var_len(value);

    while(1)
    {
        putc(buffer, out);
        if(buffer&0x80)
        {
            buffer >>=8;
        }
        else
        {
            break;
        }
    }
}

void handle_delay(FILE * out)
{
  int help=0;
  
  if(delay<0)
  {
    puts("Delay was negative!");
    help=delay;
    delay=0;
  }
  
    if(delay<=0x7F)
    {
        write_var_len(delay,out);
        tracksz[tracknum]+=1;
    }
    else if(delay<=0x3FFF)
    {
        write_var_len(delay,out);
        tracksz[tracknum]+=2;
    }
    else if(delay<=0x1FFFFF)
    {
        write_var_len(delay,out);
        tracksz[tracknum]+=3;
    }
    else if(delay<=0xFFFFFFF)
    {
        write_var_len(delay,out);
        tracksz[tracknum]+=4;
    }

    delay=help;
}

void write_volume(uint8_t vol, FILE* out)
{
    handle_delay(out);

    putc(midi_status_control_change(tracknum), out);
    putc(0x07, out); //TODO: unsure whether using expression instead of volume change
    putc(vol&0x7f, out);

    tracksz[tracknum]+=3;
}

void write_pan(uint8_t pan, FILE*out)
{
    handle_delay(out);

    putc(midi_status_control_change(tracknum), out);
    putc(0x0A, out);
    putc(pan&0x7f, out);

    tracksz[tracknum]+=3;
}

void write_bank(uint16_t bank, FILE*out)
{
    handle_delay(out);

    putc(midi_status_control_change(tracknum), out);
    putc(0x0, out); // bank coarse
    putc(bank/128, out);


    handle_delay(out);

    putc(midi_status_control_change(tracknum), out);
    putc(0x20, out); // bank fine
    putc(bank%128, out);

    tracksz[tracknum]+=6;
}

// TODO: write all interpolated events to a separate midi track, which overlays the track containing
// the midi notes, and by that get completly independent of delay handling/fixing issues, see below
void write_ctrl_interpolation(enum ctrl_type type, uint8_t const value, uint8_t duration, FILE* out)
{
    static uint8_t last_vol[TRACKS]= {0};
    static uint8_t last_pan[TRACKS]= {0};

    switch(type)
    {
    case VOLUME:
    {
        uint8_t& oldvol = last_vol[tracknum];
        int8_t diff = value - oldvol;

        if(diff==0)
        {
            // noting to do
            break;
        }
        if(duration>0)
	{
        float step = (float)diff/duration;

        // write volume change interpolation step by step
        for(float i = oldvol+step; (oldvol<value) ? (i<value) : (i>value); i+=step)
        {
            write_volume((uint8_t)i, out);
            delay=1;
        }
	}
        // write final volume state
        write_volume((uint8_t)value, out);


        oldvol = value;
    }
    break;
    case PAN:
    {
        uint8_t& oldpan = last_pan[tracknum];
        int8_t diff = value - oldpan;

        if(diff==0)
        {
            // noting to do
            break;
        }
        if(duration>0)
	{
        float step = (float)diff/duration;

        // write pan position change interpolation step by step
        for(float i = oldpan+step; (oldpan<value) ? (i<value) : (i>value); i+=step)
        {
            write_pan((uint8_t)i, out);
            delay=1;
        }
	}
        // write final pan position state
        write_pan((uint8_t)value, out);

        oldpan = value;
    }
    break;
    }

    // all those interpolated events cause a delay in the resulting midi file, that actually doesnt exist
    // to avoid that follwing events are influenced by that, and thus pushed into "the future", set delay negative
    delay=-duration;
}

int parse_ev(FILE * in, FILE * out)
{
    unsigned char ev = getc(in);
    if(ev<0x80) // note on
    {
        handle_delay(out);

        putc(midi_status_note_on(),out);
        unsigned char note = ev & 0xFF;
        putc(note,out);
        int ppid = getc(in);
        notes[ppid]=note;
        unsigned char vol = getc(in);
        putc(vol,out);

        tracksz[tracknum]+=3;
    }
    else
    {
        switch(ev)
        {
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0x87:
        {   // note off
            handle_delay(out);

            putc(midi_status_note_off(),out);
            unsigned char note = notes[ev&7];
            putc(note,out);
            putc(0,out);

            tracksz[tracknum]+=3;
        }
        break;
        case 0x80:
            if(inmain==1) basedelay+=getc(in);
            else delay+=getc(in);
            break;
        case 0x88:
        {
            if(inmain==1)
            {
                basedelay += (getc(in)<<8) + getc(in);
            }
            else
            {
                delay += (getc(in)<<8) + getc(in);
            }
        }
        break;
        case 0xF0:
        {
            int value = getc(in);
            while(value&0x80)
            {
                value=(value&0x7F)<<7;
                value+=getc(in);
            }
            if(inmain==1) basedelay += value;
            else delay += value;
        }
        break;
        case 0x9A:
        {
            ev = getc(in);

            if(ev==0x03) // pan position change event!
            {
                // from 00 (fully left) to 7F (fully right pan)
                unsigned char pan_position = getc(in);

                // usually 0x0A
                unsigned char duration = getc(in);
#ifdef DEBUG
                if(duration!=0)
                {
                    printf("pan position change duration in track %u is: %u\n", tracknum, duration);
                }
#endif
                write_ctrl_interpolation(PAN, pan_position, duration, out);
            }
            else
            {
                fseek(in,2,SEEK_CUR);
            }
        }
        break;
        case 0x9C:
        {
            ev = getc(in);

            if(ev==0x00) // volume change! (used BlueDemo.bms (=Bogmire Intro) and Title to verify)
            {
                // this can be compared to what "Expression" in MIDI is used for, dont know wether there is another preamp volume event in BMS
                // up to 7F!
                unsigned char volume = getc(in);

                // usually 0x00
                unsigned char duration = getc(in);
#ifdef DEBUG
                if(duration!=0)
                {
                    printf("volume change duration in track %u is: %u\n", tracknum, duration);
                }
#endif
                write_ctrl_interpolation(VOLUME, volume, duration, out);
            }
            else if(ev==0x09) // vibrato intensity event? pitch sensitivity event??
            {
                // scale yet unknown
                unsigned char somethingWithPitch = getc(in);

                // usually 0x00, but can be pretty much anything
                unsigned char dontknow = getc(in);
            }
            else
            {
                fseek(in,2,SEEK_CUR);
            }
        }
        break;
        case 0x9E:
        {
            ev = getc(in);

            if(ev==0x01) // pitch event
            {
                handle_delay(out);

                int16_t pitch = (getc(in) << 8) | getc(in); // TODO: verify the this is correct byte order

                // usually 0x04
                unsigned char duration = getc(in);
#ifdef DEBUG
                if(duration!=0)
                {
                    printf("pitch change duration in track %u is: %u\n", tracknum, duration);
                }
#endif
                putc(midi_status_pitch_wheel(), out);

                // TODO: before writing to file, correctly convert pitch value
                putc(pitch&0x7f,out);
                putc((pitch>>8)&0x7f,out);

                tracksz[tracknum]+=3;
            }
            else
            {
                fseek(in,3,SEEK_CUR);
            }
        }
        break;
        case 0xA4:
        {
            ev = getc(in);

            if(ev==0x21) // instrument/program change event
            {
                handle_delay(out);

                // scale yet unknown
                unsigned char program = getc(in);

                putc(midi_status_prog_change(), out);
                putc(program&0x7f,out);

                tracksz[tracknum]+=2;
            }
            else if(ev==0x20) // bank selection (pretty sure)
            {
                unsigned char bank = getc(in);

                write_bank(bank, out);
            }
            else if(ev==0x07)
            {
                //DOnt know
                fseek(in,1,SEEK_CUR);
            }
            else
            {
                fseek(in,1,SEEK_CUR);
            }
        }
        break;
        case 0xB1:
        {
            fseek(in,1,SEEK_CUR);
            int flag = getc(in);
            if(flag==0x40) fseek(in,2,SEEK_CUR);
            else if(flag==0x80) fseek(in,4,SEEK_CUR);
        }
        break;

        case 0xC2:
        /*fall through*/
        case 0xC6:
        /*fall through*/
        case 0xCF:
        /*fall through*/
        case 0xDA:
        /*fall through*/
        case 0xDB:
        /*fall through*/
        case 0xE2: // NEW!
        /*fall through*/
        case 0xE3: // NEW!
        /*fall through*/
        case 0xF1:
        /*fall through*/
        case 0xF4:
            fseek(in,1,SEEK_CUR);
            break;

        case 0x98:
        /*fall through*/
        case 0xA0:
        /*fall through*/
        case 0xA3:
        /*fall through*/
        case 0xA5:
        /*fall through*/
        case 0xA7:
        /*fall through*/
        case 0xB8:
        /*fall through*/
        case 0xCB:
        /*fall through*/
        case 0xCC:
        /*fall through*/
        case 0xD0:
        /*fall through*/
        case 0xD1:
        /*fall through*/
        case 0xD2:
        /*fall through*/
        case 0xD5:
        /*fall through*/
        case 0xE0:
        /*fall through*/
        case 0xE6:
        /*fall through*/
        case 0xE7:
        /*fall through*/
        case 0xF9:
            fseek(in,2,SEEK_CUR);
            break;

        case 0xAC:
        /*fall through*/
        case 0xAD:
        /*fall through*/
        case 0xC5:
        /*fall through*/
        case 0xD8:// NEW!
        /*fall through*/
        case 0xDD:
        /*fall through*/
        case 0xEF:
            fseek(in,3,SEEK_CUR);
            break;

        case 0xA9:
        /*fall through*/
        case 0xAA:
        /*fall through*/
        case 0xC4:
        /*fall through*/
        case 0xC7:
        /*fall through*/
        case 0xC8:
        /*fall through*/
        case 0xDF:
            fseek(in,4,SEEK_CUR);
            break;

        case 0xFD:
        {
            // TODO: support tempo change throughout track, not just as initialization
            if(tempo==0)
            {
                tempo = (getc(in)<<8) | getc(in);
            }
            else
            {
                puts("This BMS is using Tempo Change Events, which is not yet implemented.");
                fseek(in,2,SEEK_CUR);
            }
#ifdef DEBUG
            if(inmain==1)
                puts("tempo change: is in main!");
            else
                puts("tempo change not in main");
            puts("");
#endif
        }
        break;
        case 0xFE:
        {
            if(ppqn==0) // unset
            {
                ppqn = (getc(in)<<8) | getc(in);
            }
            else
            {
                puts("PPQN already set and not supported as change event. Ignoring it.");
                fseek(in,2,SEEK_CUR);
            }
        }
        break;

        case 0xC1:
            return BR_C1;
        case 0xFF:
            return BR_FF;
        default:
            printf("Warning: Event %d (0x%X) unhandled\n", ev, ev);
            break;
        }
    }
    return BR_NORMAL;
}

void usage(char* progName)
{
    printf("Usage: %s in.bms out.midi\n", progName);
}

int main(int argc, char ** argv)
{
    if(argc != 3)
    {
        usage(argv[0]);
        return -1;
    }

    FILE * fp = fopen(argv[1],"rb");
    FILE * out = fopen("TEMP","wb");
    while(true)
    {
        int status = parse_ev(fp,out);
        if(status==BR_NORMAL);
        else if(status==BR_C1)
        {
            // this could be the channel for the corresponding track
            current_channel = getc(fp);
	    if (current_channel >=16)
	    {
	      fprintf(stderr, "Error: BMS contains more than 15 channels! Exiting.");
	      return -1;
	    }
	    
            long offset = (getc(fp)<<16) + (getc(fp)<<8) + getc(fp);
            savepos=ftell(fp);
            fseek(fp,offset,SEEK_SET);
            inmain=0;
        }
        else if(status==BR_FF)
        {
            if(inmain==1) break;
            else
            {
                tracksz[tracknum]+=4;
                putc(0,out);
                putc(0xFF,out);
                putc(0x2F,out);
                putc(0,out);

                delay=basedelay;
                fseek(fp,savepos,SEEK_SET);
                tracknum++;

                if(tracknum > TRACKS)
                {
                    fprintf(stderr, "Error: BMS contains more than %d tracks! Exiting.",TRACKS);
                    return -1;
                }

                inmain=1;
            }
        }
    }
    fclose(fp);
    fclose(out);

    FILE * midi_file = fopen(argv[2],"wb");
    putc('M',midi_file);
    putc('T',midi_file);
    putc('h',midi_file);
    putc('d',midi_file);

    // midi header length
    putc(0,midi_file);
    putc(0,midi_file);
    putc(0,midi_file);
    putc(6,midi_file);

    // SMF1
    putc(0,midi_file);
    putc(1,midi_file);

    // number of tracks
    putc(0,midi_file);
    putc(tracknum,midi_file);

    // ppqn
    if(ppqn==0) // if unset
    {
        puts("BMS doesnt specify PPQN. Defaulting to 96.");
        ppqn=96;
    }
    putc((ppqn>>8)&0xFF,midi_file);
    putc(ppqn&0xFF,midi_file);

    // write tracks
    out = fopen("TEMP","rb");

    if(tempo==0)
    {
        puts("BMS doesnt specify a Tempo. Imply 120 BPM.");
    }
    else
    {
        // write meta track (i.e. for tempo change only)
        putc('M',midi_file);
        putc('T',midi_file);
        putc('r',midi_file);
        putc('k',midi_file);

        // tracklength
        putc(0,midi_file);
        putc(0,midi_file);
        putc(0,midi_file);
        putc(11,midi_file);

        // 0 delay
        write_var_len(0,midi_file);
        // tempo change event
        putc(0xFF,midi_file);
        putc(0x51,midi_file);
        putc(0x03,midi_file);

        if(tempo<=3)
        {
            puts("Tempo slower 4 BPM are not supported by MIDI spec. Setting to slowest possible.");
            putc(0xFF,midi_file);
            putc(0xFF,midi_file);
            putc(0xFF,midi_file);
        }
        else // most sig. byte should be 0 so we can write tempo to 3 bytes
        {
            // micro seconds per quarter note
            uint32_t usec_pqn=(uint32_t)((60/(float)tempo) * 1000 * 1000);
            putc((usec_pqn>>16) & 0xFF,midi_file);
            putc((usec_pqn>>8) & 0xFF,midi_file);
            putc((usec_pqn) & 0xFF,midi_file);
        }

        // 0 delay
        write_var_len(0,midi_file);
        // end of track
        putc(0xFF,midi_file);
        putc(0x2F,midi_file);
        putc(0,midi_file);
    }

    for(int i=0; i<tracknum; i++)
    {
        putc('M',midi_file);
        putc('T',midi_file);
        putc('r',midi_file);
        putc('k',midi_file);

        // track chunk length - bigEndian
        putc(0,midi_file);
        putc((tracksz[i]&0xFF0000)>>16,midi_file);
        putc((tracksz[i]&0xFF00)>>8,midi_file);
        putc(tracksz[i]&0xFF,midi_file);

        for(int j=0; j<tracksz[i]; j++)
        {
            int w = getc(out);
            putc(w,midi_file);
        }
    }
    fclose(midi_file);
    fclose(out);
    return 0;
}
