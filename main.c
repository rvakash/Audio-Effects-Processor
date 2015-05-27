//Header files
#include "ulk_base_types.h"
#include "ulk_io.h"
#include "ulk_proc_timer.h"
#include "ulk_proc_audio.h"
#include "ulk_proc_sdma.h"
#include "ulk_error.h"
#include "raise.h"
#include <ulk.h>
#include "string.h"
#include "macros.h"

//LCD definations
#define GLCD_ROWS 240
#define GLCD_COLUMNS 320
#define LINE_WIDTH 2
#define GRID_ROWS 5
#define GRID_COLUMNS 5
#define GLCD_MEMORY 0x80500000  //pointer to touch memory
#define WHITE_COLOR 0x00ffffff
#define BLACK_COLOR 0x00000000

int y = GLCD_COLUMNS/GRID_COLUMNS;
int x = GLCD_ROWS/GRID_ROWS;

//Touch co-ordinates (x,y)
struct PIXEL
{
		unsigned int x;
		unsigned int y;
};

extern struct PIXEL pixel;
extern struct PIXEL ulk_proc_touch_spi_enable(void);
extern struct PIXEL ulk_proc_touch_spi_poll(void);

//function prototypes
void draw_horizontal_line(int);
void draw_vertical_line(int);
void draw_grid(int, int);
void draw_fillbox_black();
void draw_fill();

unsigned long *ptr = GLCD_MEMORY;
int main(void) PROGRAM_ENTRY;

void ulk_sdma_isr1();
void ulk_sdma_isr2();

//buffer to handle recorded audio data
uint16 buf[48000*60*2] ;

//temp buffer to store delayed audio data
uint16 echo[48000*60*2];
uint16 echo_temp[48000*60*2];
uint16 echo_temp25[48000*60*2];

//final buffer to play echo effect
uint16 echo_final[48000*60*2];
uint16 echo_final2[48000*60*2];
uint16 echo_final25[48000*60*2];

//buffer to store processed data (tempo effect) 
uint16 volume[44000*10];

//default audio settings
ulk_audio_config_t ulk_audio_config = {0, 0, 0, 0, 0, 0, 0};
ulk_audio_pcm_t ulk_audio_pcm ;
int g=0;

/* The following function draws a horizontal line at the
 * specified row number, passed as argument
 */
draw_horizontal_line(int row)
{
	int i;
	for(i=GLCD_COLUMNS*row; i<GLCD_COLUMNS*(row+LINE_WIDTH); i++)
		*(ptr+i)=BLACK_COLOR;
}

/* The following function draw a vertical line at the specified
 * column number passed as argument
 */

draw_vertical_line(int column)
{
	int i, j, offset;
	for(j=0; j<GLCD_ROWS; j++)
	{
		for(i=0;i<LINE_WIDTH;i++)
		{
			offset = i+j*GLCD_COLUMNS+column;
			*(ptr+offset)=BLACK_COLOR;
		}

	}
}

/* The following function divides the GLCD display panel into
 * equal number of rows based on the first value passed and
 * equal number of columns based on the second value passed
 * as arguments.
 */
draw_grid(int x, int y)
{
	int horizontal_lines, vertical_lines;
	int i;
	horizontal_lines = GLCD_ROWS/x;
	for(i=0; i<(x+1); i++)
	{
		draw_horizontal_line(horizontal_lines*i);
	}
	vertical_lines = GLCD_COLUMNS/y;
	for(i=0; i<(x+1); i++)
	{
		draw_vertical_line(vertical_lines*i);
	}
}

/* The following function when called get the x-y coordinate from
 * touch panel depending on where the touch happened with
 * stylus and then find out to which box it belongs to and 
 * then fills black colour in the respective column till 
 * that box number.
 * Input Value: column,row
 * Return Value: Box number
 */


draw_fillbox_black( int q, int w)
{
	int y = GLCD_COLUMNS/GRID_COLUMNS;
	int x = GLCD_ROWS/GRID_ROWS;
	int box_col;
	int box_row;
	//ulk_cpanel_printf("y= %d \n",y);
	//ulk_cpanel_printf("q= %d \n",w);

	y = y + q;
	x = x + w;
	ulk_cpanel_printf("y= %d \n",y);
	ulk_cpanel_printf("q= %d \n",q);
	ulk_cpanel_printf("x= %d \n",x);
	ulk_cpanel_printf("w= %d \n",w);

			for(box_col=q;box_col<y;box_col++)
			{
				int org = 0;
				*(ptr+box_col) = WHITE_COLOR;
				for(box_row=0;box_row<244;box_row++)
				{
					org=org+320;
					*(ptr+org+box_col)= WHITE_COLOR;
				}
			}
			for(box_col=q;box_col<y;box_col++)
			{
				int org=0;
				*(ptr+box_col)= BLACK_COLOR;
				for(box_row=0;box_row<x;box_row++)
				{
					org=org+320;
				    *(ptr+org+box_col)= BLACK_COLOR;
				}
			}
}


//Main method
int32 main ()
{
        //enabling the audio interface
	ulk_proc_audio_init();
	ulk_proc_get_dflt_config(&ulk_audio_config);
	ulk_cpanel_printf("default(init) settings used for microphone\r\n");
	ulk_cpanel_printf("SAMPLE_RATE=%dkhz\r\nBOOST_EFFECT=%d\r\nMIC_L_GAIN=%ddb\r\n",
			ulk_audio_config.SAMPLE_RATE,ulk_audio_config.BOOST_EFFECT,
			ulk_audio_config.MIC_L_GAIN);
	ulk_cpanel_printf("MIC_R_GAIN=%ddb\r\nSPK_L_GAIN=%ddb\r\n",
			ulk_audio_config.MIC_R_GAIN,ulk_audio_config.SPK_L_GAIN);
	ulk_cpanel_printf("SPK_R_GAIN=%ddb\r\nOUT_MODE=%s\r\n",
			ulk_audio_config.SPK_R_GAIN,(ulk_audio_config.OUT_MODE=='S')?"Stereo":"Mono");
	ulk_cpanel_printf("\r\nconfiguring new settings used for line in\r\n");
	
	//new audio settings
	ulk_audio_config.SAMPLE_RATE=44.1;
	ulk_audio_config.BOOST_EFFECT=3;
	ulk_audio_config.MIC_L_GAIN=-6;
	ulk_audio_config.MIC_R_GAIN=-6;
	ulk_audio_config.SPK_L_GAIN=10;
	ulk_audio_config.SPK_R_GAIN=10;
	ulk_audio_config.OUT_MODE='M';
	ulk_proc_audio_set_config(&ulk_audio_config);
	ulk_proc_audio_get_config(&ulk_audio_config);
	ulk_cpanel_printf("SAMPLE_RATE=%dkhz\r\nBOOST_EFFECT=%d\r\nMIC_L_GAIN=%ddb\r\n",
			ulk_audio_config.SAMPLE_RATE,ulk_audio_config.BOOST_EFFECT,
			ulk_audio_config.MIC_L_GAIN);
	ulk_cpanel_printf("MIC_R_GAIN=%ddb\r\nSPK_L_GAIN=%ddb\r\n",
			ulk_audio_config.MIC_R_GAIN,ulk_audio_config.SPK_L_GAIN);
	ulk_cpanel_printf("SPK_R_GAIN=%ddb\r\nOUT_MODE=%s\r\n",
			ulk_audio_config.SPK_R_GAIN,(ulk_audio_config.OUT_MODE=='S')?"Stereo":"Mono");
	
	//PCM audio settings
	ulk_audio_pcm.pcm_type='D';
	ulk_audio_pcm.pcm_data_p=(uint8* )buf;
	ulk_audio_pcm.pcm_size=(ulk_audio_config.SAMPLE_RATE)*1000*10;
	ulk_cpanel_printf("record: %d-samples, %s-line\r\n",ulk_audio_pcm.pcm_size,
			(ulk_audio_pcm.pcm_type=='D')?"Default":((ulk_audio_pcm.pcm_type=='R')?"Right":"Left"));
	//Record audio from input
	ulk_proc_audio_record (&ulk_audio_pcm);
	ulk_cpanel_printf("recording completed");
	
	//LCD

	int i,j;
	int s=0x00;

	//To make the complete screen painted white
	for(i=0; i < GLCD_COLUMNS*GLCD_ROWS; i++)
	{
		*(ptr+i)= WHITE_COLOR;
	}

	//Draw grid of touch panel virtual keys on the white screen
	//of GLCD for touch panel
	draw_grid(GRID_ROWS, GRID_COLUMNS);


	//Enables the touch panel
	ulk_proc_touch_spi_enable();
	// variables to hold touch co-ordinates
	int valx,valy;
	int boxnum;
		while(1)
		{
			int column = 64;//each grid width
			int row = 49;//each grid height
			valx=0;valy=0;boxnum=0;
			pixel = ulk_proc_touch_spi_poll();
					//To find box number if a touch is made
					if((pixel.x != 0) && (pixel.y != 0))
					{
						valx=pixel.x;
						valy=pixel.y -240;
						valx=valx/64+1;
						valy=-(valy)/48+1;
						boxnum=((valy-1)*5)+valx;
						ulk_cpanel_printf("x pos = %d \n",valx);
						ulk_cpanel_printf("y pos = %d \n",valy);
						ulk_cpanel_printf("Box_no = %d \n",boxnum);
						column=column*(valx-1);
						row=row*(valy-1);
						draw_fillbox_black(column,row);
						draw_grid(GRID_ROWS, GRID_COLUMNS);
						//ulk_cpanel_printf("col = %d \n",column);
						//ulk_cpanel_printf("row = %d \n",row);
						//match the box number to the respective effect
						if(boxnum==5)
							echo0();
						if(boxnum==10)
							echo25();
						if(boxnum==15)
							echo35();
						if(boxnum==25)
							echo55();
						if(boxnum==2)
							temp0();
						if(boxnum==7)
							temp0_8();
						if(boxnum==12)
							temp2();
					}

		}

		return 0;

}


void ulk_sdma_isr1()
{
	ulk_cpanel_printf("audio in finished\r\n");
}

void ulk_sdma_isr2()
{
	ulk_cpanel_printf("audio out finished\r\n");
}

/* This is a function to generate echo effect 
 * with zero sample delay
 */
void echo0()
{
	ulk_audio_pcm.pcm_type='D';
	ulk_audio_pcm.pcm_data_p=(uint8* )buf;
	ulk_cpanel_printf("\nwithoutecho_play");
	ulk_proc_audio_play (&ulk_audio_pcm);
	ulk_cpanel_printf("\nwithoutecho_play completed");
}

/* This is a function to generate echo effect 
 * with 25000 sample delay
 * Or 0.56 second delay
 */
void echo25()
{
	int size=sizeof(buf)/sizeof(buf[0]);
		for(g=0; g < 25000; g++)  
		{	
			echo_temp25[g] = 0;
		}

		for(g=0; g < size; g++)
		{
			echo_temp25[g+25000] = buf[g];
		}

		for(g=0; g < size; g++)
		{
			echo_final25[g] = buf[g]+echo_temp25[g];
		}

		ulk_audio_pcm.pcm_data_p=(uint8* )echo_final25;
		ulk_audio_pcm.pcm_type='D';
		ulk_cpanel_printf("\necho25_play");
		ulk_proc_audio_play (&ulk_audio_pcm);
		ulk_cpanel_printf("\necho25_play completed");

}

/* This is a function to generate echo effect 
 * with 35000 sample delay
 * Or 0.79 second delay
 */
void echo35()
{
		int size=sizeof(buf)/sizeof(buf[0]);
		for(g=0; g < 35000; g++)  //ECHO till specific time is 0
		{
			echo[g] = 0;
		}

		for(g=0; g < size; g++)
		{
			echo[g+35000] = buf[g];
		}

		for(g=0; g < size; g++)
		{
			echo_final[g] = buf[g]+echo[g];
		}

		ulk_audio_pcm.pcm_data_p=(uint8* )echo_final;
		ulk_audio_pcm.pcm_type='D';
		ulk_cpanel_printf("\necho35_play");
		ulk_proc_audio_play (&ulk_audio_pcm);
		ulk_cpanel_printf("\necho35_play completed");
}

/* This is a function to generate echo effect 
 * with 45000 sample delay
 */
void echo45()
{
/*	for(g=0; g < 45000; g++)  //ECHO till specific time is 0
			{
				echo_temp45[g] = 0;
			}

			for(g=0; g < size; g++)
			{
				echo_temp45[g+45000] = buf[g];
			}

			for(g=0; g < size; g++)
			{
				echo_final45[g] = buf[g]+echo[g];
			}

			ulk_audio_pcm.pcm_data_p=(uint8* )echo_final45;
			ulk_audio_pcm.pcm_type='D';
			ulk_cpanel_printf("\necho_play");
			ulk_proc_audio_play (&ulk_audio_pcm);
*/
}

/* This is a function to generate echo effect 
 * with 55000 sample delay
 * Or 1.22 second delay
 */
void echo55()
{
	int size=sizeof(buf)/sizeof(buf[0]);

			for(g=0; g < 55000; g++)  //ECHO till specific time is 0
			{
				echo_temp[g] = 0;
			}

			for(g=0; g < size; g++)
			{
				echo_temp[g+55000] = buf[g];
			}

			for(g=0; g < size; g++)
			{
				echo_final2[g] = buf[g]+echo_temp[g];
			}

			ulk_audio_pcm.pcm_data_p=(uint8* )echo_final2;
			ulk_audio_pcm.pcm_type='D';
			ulk_cpanel_printf("\necho55_play");
			ulk_proc_audio_play (&ulk_audio_pcm);
			ulk_cpanel_printf("\necho55_play completed");

}

/* This is a function to generate tempo effect 
 * with zero speed
 */
void temp0()
{
	int size=sizeof(buf)/sizeof(buf[0]);
	int j=0;
	for(g=0;g<size;g++)
{
		j=g*1;
		volume[g]=buf[j];
}

	ulk_audio_pcm.pcm_type='D';
	ulk_audio_pcm.pcm_data_p=(uint8* )volume;
	ulk_cpanel_printf("\ntemp0_play");
	ulk_proc_audio_play (&ulk_audio_pcm);
	ulk_cpanel_printf("\ntemp0_play completed");
}

/* This is a function to generate tempo effect  
 * with slowing up of the playback 
 */
void temp0_8()
{
	int size=sizeof(buf)/sizeof(buf[0]);
	int j=0;
	for(g=0;g<size;g++)
{
		j=g*0.8;
		volume[g]=buf[j];
}

	ulk_audio_pcm.pcm_type='D';
	ulk_audio_pcm.pcm_data_p=(uint8* )volume;
	ulk_cpanel_printf("\ntemp0.8_play");
	ulk_proc_audio_play (&ulk_audio_pcm);
	ulk_cpanel_printf("\ntemp0.8_play completed");
}

/* This is a function to generate tempo effect  
 * with speeding up of the playback 
 */
void temp2()
{
	int size=sizeof(buf)/sizeof(buf[0]);
	int j=0;
	for(g=0;g<size;g++)
{
		j=g*2;
		volume[g]=buf[j];
}

	ulk_audio_pcm.pcm_type='D';
	ulk_audio_pcm.pcm_data_p=(uint8* )volume;
	ulk_cpanel_printf("\ntemp2_play");
	ulk_proc_audio_play (&ulk_audio_pcm);
	ulk_cpanel_printf("\ntemp2_play completed");
}
