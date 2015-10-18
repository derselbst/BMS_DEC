#include <stdio.h>

int notes[8];
int tracknum=0;
int delay=0;
int basedelay=0;
int tracksz[16]={0};
int savepos=0;
int inmain=1;
int branchindex=0;
int branchstack[16]={0};

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
			else if(ev==0xB8) fseek(in,2,SEEK_CUR);
			else if(ev==0xB9) fseek(in,3,SEEK_CUR);
			else if(ev==0xC1) return BR_C1;
			else if(ev==0xC2) fseek(in,1,SEEK_CUR);
			else if(ev==0xC3)
			{
				branchstack[branchindex]=ftell(in)+3;
				int gotome = (getc(in)<<16) + (getc(in)<<8) + getc(in);
				fseek(in,gotome,SEEK_SET);
				branchindex++;
			}
			else if(ev==0xC4) fseek(in,4,SEEK_CUR);
			else if(ev==0xC5)
			{
				if(branchindex==0) return BR_FF;
				else
				{
					branchindex--;
					fseek(in,branchstack[branchindex],SEEK_SET);
				}
			}
			else if(ev==0xC7) fseek(in,3,SEEK_CUR);
			else if(ev==0xD0) fseek(in,2,SEEK_CUR);
			else if(ev==0xD1) fseek(in,2,SEEK_CUR);
			else if(ev==0xD5) fseek(in,3,SEEK_CUR);
			else if(ev==0xD8) fseek(in,3,SEEK_CUR);
			else if(ev==0xD9) fseek(in,3,SEEK_CUR);
			else if(ev==0xDA) fseek(in,4,SEEK_CUR);
			else if(ev==0xE2) fseek(in,1,SEEK_CUR);
			else if(ev==0xE3) fseek(in,1,SEEK_CUR);
			else if(ev==0xF0)
			{
				int get = getc(in);
				int value = (get&0x7F);
				while(get&0x80)
				{
					value<<=7;
					get=getc(in);
					value+=(get&0x7F);
				}
				if(inmain==1) basedelay += value;
				else delay += value;
			}
			else if(ev==0xE0) fseek(in,2,SEEK_CUR);
			else if(ev==0xF9) fseek(in,2,SEEK_CUR);
			//else if(ev==0xFD);
			else if(ev==0xFF) return BR_FF;
			return BR_NORMAL;
}

int main(int argc, char ** argv)
{
	FILE * fp = fopen(argv[1],"rb");
	FILE * out = fopen("TEMP","wb");
	FILE * fp2 = fopen(argv[2],"wb");
	/*for DKJB BMS files*/
	fseek(fp,3,SEEK_SET);
	int initial = (getc(fp)<<8) + getc(fp);
	fseek(fp,initial,SEEK_SET);
	/*~~~~~~~~~~~~~~~~~~*/
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
