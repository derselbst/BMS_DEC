#include <stdio.h>
#include <stdint.h>

unsigned char notes[8];
unsigned char tracknum=0;
int delay=0;
int basedelay=0;
int tracksz[16]= {0};
int savepos=0;
int inmain=1;

enum branch
{
    BR_NORMAL,
    BR_C1,
    BR_FF
};

unsigned char midi_status_note_on(unsigned char chan)
{
    // only lower nibble for channel specification
    chan &= 0b00001111;

    chan |= 0b1001 << 4;

    return chan;
}

unsigned char midi_status_note_off(unsigned char chan)
{
    // only lower nibble for channel specification
    chan &= 0b00001111;

    chan |= 0b1000 << 4;

    return chan;
}

unsigned char midi_status_pitch_wheel(unsigned char chan)
{
    // only lower nibble for channel specification
    chan &= 0b00001111;

    chan |= 0b1110 << 4;

    return chan;
}

unsigned char midi_status_prog_change(unsigned char chan)
{
    // only lower nibble for channel specification
    chan &= 0b00001111;

    chan |= 0b1100 << 4;

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
    if(delay<=0x7F)
    {
        write_var_len(delay,out);
        tracksz[tracknum]+=4;
    }
    else if(delay<=0x3FFF)
    {
        write_var_len(delay,out);
        tracksz[tracknum]+=5;
    }
    else if(delay<=0x1FFFFF)
    {
        write_var_len(delay,out);
        tracksz[tracknum]+=6;
    }
    else if(delay<=0xFFFFFFF)
    {
        write_var_len(delay,out);
        tracksz[tracknum]+=7;
    }
}

int parse_ev(FILE * in, FILE * out)
{
    unsigned char ev = getc(in);
    if(ev<0x80) // note on
    {
        handle_delay(out);

        putc(midi_status_note_on(tracknum),out);
        unsigned char note = ev & 0xFF;
        putc(note,out);
        int ppid = getc(in);
        notes[ppid]=note;
        unsigned char vol = getc(in);
        putc(vol,out);
        delay=0;
    }
    else if(ev==0x80)
    {
        if(inmain==1) basedelay+=getc(in);
        else delay+=getc(in);
    }
    else if(ev<0x88) // note off
    {
        handle_delay(out);

        putc(midi_status_note_off(tracknum),out);
        unsigned char note = notes[ev&7];
        putc(note,out);
        putc(0,out);
        delay=0;
    }
    else if(ev==0x88)
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
    else if(ev==0x98) fseek(in,2,SEEK_CUR);
    else if(ev==0x9A)
    {
        ev = getc(in);

        if(ev==0x03) // pan position change event!
        {
            // from 00 (fully left) to 7F (fully right pan)
            unsigned char pan_position = getc(in);

            // always 0x0A ???
            unsigned char dontknow = getc(in);
        }
        else
        {
            fseek(in,2,SEEK_CUR);
        }
    }
    else if(ev==0x9C)
    {
        ev = getc(in);

        if(ev==0x00) // volume change! (used BlueDemo.bms (=Bogmire Intro) and Title to verify)
        {
            // this can be compared to what "Expression" in MIDI is used for, dont know wether there is another preamp volume event in BMS
            // up to 7F!
            unsigned char volume = getc(in);

            // always 0x00
            unsigned char dontknow = getc(in);
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
    else if(ev==0x9E)
    {
      ev = getc(in);
      
      if(ev==0x01) // pitch event
      {
        handle_delay(out);
	
	int16_t pitch = (getc(in) << 8) | getc(in); // TODO: verify the this is correct byte order
	
	// always 0x04??
	unsigned char dontknow = getc(in);
	
	putc(midi_status_pitch_wheel(tracknum), out);
        putc(pitch&0x7f,out);
	putc((pitch>>8)&0x7f,out);
	
      }
      else
      {
	fseek(in,3,SEEK_CUR);
      }
    }
    else if(ev==0xA0) fseek(in,2,SEEK_CUR);
    else if(ev==0xA3) fseek(in,2,SEEK_CUR);
    else if(ev==0xA4)
    {
      ev = getc(in);
      
      if(ev==0x21) // instrument/program change event
      {
        handle_delay(out);
	
	// scale yet unknown
	unsigned char program = getc(in);
	
	putc(midi_status_prog_change(tracknum), out);
        putc(program&0x7f,out);
      }
      else if(ev==0x20) // bank selection (pretty sure)
      {
	unsigned char bank = getc(in);
      }
      else if(ev==0x07)
      {
	//DOnt know
      }
      else
      {
      fseek(in,1,SEEK_CUR);
      }
    }
    else if(ev==0xA5) fseek(in,2,SEEK_CUR);
    else if(ev==0xA7) fseek(in,2,SEEK_CUR);
    else if(ev==0xA9) fseek(in,4,SEEK_CUR);
    else if(ev==0xAA) fseek(in,4,SEEK_CUR);
    else if(ev==0xAC) fseek(in,3,SEEK_CUR);
    else if(ev==0xAD) fseek(in,3,SEEK_CUR);
    else if(ev==0xB1)
    {
        fseek(in,1,SEEK_CUR);
        int flag = getc(in);
        if(flag==0x40) fseek(in,2,SEEK_CUR);
        else if(flag==0x80) fseek(in,4,SEEK_CUR);
    }
    else if(ev==0xB8) fseek(in,2,SEEK_CUR);
    else if(ev==0xC1) return BR_C1;
    else if(ev==0xC2) fseek(in,1,SEEK_CUR);
    else if(ev==0xC4) fseek(in,4,SEEK_CUR);
    else if(ev==0xC5) fseek(in,3,SEEK_CUR);
    else if(ev==0xC6) fseek(in,1,SEEK_CUR);
    else if(ev==0xC7) fseek(in,4,SEEK_CUR);
    else if(ev==0xC8) fseek(in,4,SEEK_CUR);
    else if(ev==0xCB) fseek(in,2,SEEK_CUR);
    else if(ev==0xCC) fseek(in,2,SEEK_CUR);
    else if(ev==0xCF) fseek(in,1,SEEK_CUR);
    else if(ev==0xD0) fseek(in,2,SEEK_CUR);
    else if(ev==0xD1) fseek(in,2,SEEK_CUR);
    else if(ev==0xD2) fseek(in,2,SEEK_CUR);
    else if(ev==0xD5) fseek(in,2,SEEK_CUR);
    else if(ev==0xD8) fseek(in,3,SEEK_CUR); // NEW!
    else if(ev==0xDA) fseek(in,1,SEEK_CUR);
    else if(ev==0xDB) fseek(in,1,SEEK_CUR);
    else if(ev==0xDD) fseek(in,3,SEEK_CUR);
    else if(ev==0xDF) fseek(in,4,SEEK_CUR);
    else if(ev==0xE0) fseek(in,2,SEEK_CUR); // WAS 3
    else if(ev==0xE2) fseek(in,1,SEEK_CUR); // NEW!
    else if(ev==0xE3) fseek(in,1,SEEK_CUR); // NEW!
    else if(ev==0xE6) fseek(in,2,SEEK_CUR);
    else if(ev==0xE7) fseek(in,2,SEEK_CUR);
    else if(ev==0xEF) fseek(in,3,SEEK_CUR);
    else if(ev==0xF0)
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
    else if(ev==0xF1) fseek(in,1,SEEK_CUR);
    else if(ev==0xF4) fseek(in,1,SEEK_CUR);
    else if(ev==0xF9) fseek(in,2,SEEK_CUR);
    else if(ev==0xFD) fseek(in,2/*could be an int16 (bigEndian?)*/,SEEK_CUR);
    else if(ev==0xFE) fseek(in,2,SEEK_CUR);
    else if(ev==0xFF) return BR_FF;
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
            fseek(fp,1,SEEK_CUR);
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

                if(tracknum >=16)
                {
                    fprintf(stderr, "Error: BMS contains more than 16 tracks! Exiting.");
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

    // 120 BPM
    putc(0,midi_file);
    putc(120,midi_file);

    // write tracks
    out = fopen("TEMP","rb");
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
