#include <stdio.h>

int notes[8];
int tracknum=0;
int delay=0;
int basedelay=0;
int tracksz[16]={0};
int savepos=0;
int inmain=1;

enum branch
{
	BR_NORMAL,
	BR_C1,
	BR_FF
};

int parse_ev(FILE * in, FILE * out)
{
			int ev = getc(in);
			if(ev<0x80)
			{
				if(delay<=0x7F)
				{
					putc(delay,out);
					tracksz[tracknum]+=4;
				}
				else if(delay<=0x3FFF)
				{
					putc(0x80+(delay>>7),out);
					putc(delay&0x7F,out);
					tracksz[tracknum]+=5;
				}
				else if(delay<=0x1FFFFF)
				{
					putc(0x80+(delay>>14),out);
					putc(0x80+((delay>>7)&0x7F),out);
					putc(delay&0x7F,out);
					tracksz[tracknum]+=6;
				}
				else if(delay<=0xFFFFFFF)
				{
					putc(0x80+(delay>>21),out);
					putc(0x80+((delay>>14)&0x7F),out);
					putc(0x80+((delay>>7)&0x7F),out);
					putc(delay&0x7F,out);
					tracksz[tracknum]+=7;
				}
				putc(0x90,out);
				int note = ev;
				putc(note,out);
				int ppid = getc(in);
				notes[ppid]=note;
				int vol = getc(in);
				putc(vol,out);
				delay=0;
			}
			else if(ev==0x80)
			{
				if(inmain==1) basedelay+=getc(in);
				else delay+=getc(in);
			}
			else if(ev<0x88)
			{
				if(delay<=0x7F)
				{
					putc(delay,out);
					tracksz[tracknum]+=4;
				}
				else if(delay<=0x3FFF)
				{
					putc(0x80+(delay>>7),out);
					putc(delay&0x7F,out);
					tracksz[tracknum]+=5;
				}
				else if(delay<=0x1FFFFF)
				{
					putc(0x80+(delay>>14),out);
					putc(0x80+((delay>>7)&0x7F),out);
					putc(delay&0x7F,out);
					tracksz[tracknum]+=6;
				}
				else if(delay<=0xFFFFFFF)
				{
					putc(0x80+(delay>>21),out);
					putc(0x80+((delay>>14)&0x7F),out);
					putc(0x80+((delay>>7)&0x7F),out);
					putc(delay&0x7F,out);
					tracksz[tracknum]+=7;
				}
				putc(0x80,out);
				int note = notes[ev&7];
				putc(note,out);
				putc(0,out);
				delay=0;
			}
			else if(ev==0x88)
			{
				if(inmain==1) {basedelay += (getc(in)<<8) + getc(in);}
				else {delay += (getc(in)<<8) + getc(in);}
			}
			else if(ev==0x98) fseek(in,2,SEEK_CUR);
			else if(ev==0x9A) fseek(in,3,SEEK_CUR);
			else if(ev==0x9C) fseek(in,3,SEEK_CUR);
			else if(ev==0x9E) fseek(in,4,SEEK_CUR);
			else if(ev==0xA0) fseek(in,2,SEEK_CUR);
			else if(ev==0xA3) fseek(in,2,SEEK_CUR);
			else if(ev==0xA4) fseek(in,2,SEEK_CUR);
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
			else if(ev==0xFD) fseek(in,2,SEEK_CUR);
			else if(ev==0xFE) fseek(in,2,SEEK_CUR);
			else if(ev==0xFF) return BR_FF;
			return BR_NORMAL;
}

int main(int argc, char ** argv)
{
	FILE * fp = fopen(argv[1],"rb");
	FILE * out = fopen("TEMP","wb");
	FILE * fp2 = fopen(argv[2],"wb");
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
				inmain=1;
			}
		}
	}
	fclose(fp);
	fclose(out);
	out = fopen("TEMP","rb");
	putc('M',fp2);
	putc('T',fp2);
	putc('h',fp2);
	putc('d',fp2);
	putc(0,fp2);
	putc(0,fp2);
	putc(0,fp2);
	putc(6,fp2);
	putc(0,fp2);
	putc(1,fp2);
	putc(0,fp2);
	putc(tracknum,fp2);
	putc(0,fp2);
	putc(120,fp2);
	for(int i=0; i<tracknum; i++)
	{
		putc('M',fp2);
		putc('T',fp2);
		putc('r',fp2);
		putc('k',fp2);
		putc(0,fp2);
		putc((tracksz[i]&0xFF0000)>>16,fp2);
		putc((tracksz[i]&0xFF00)>>8,fp2);
		putc(tracksz[i]&0xFF,fp2);
		for(int j=0; j<tracksz[i]; j++)
		{
			int w = getc(out);
			putc(w,fp2);
		}
	}
	fclose(fp2);
	fclose(out);
	return 0;
}
