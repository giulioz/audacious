/*
 * MP4/AAC decoder for xmms
 *
 * This decoding source code is completly independent of the faad2
 * package.
 * This package exist for people who don't want to install
 * faad2 and mpeg4ip project files.
 *
 * OPTIONNAL need
 * --------------
 * libid3 (3.8.x - www.id3.org)
*/

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "faad.h"
#include "mp4.h"
#include "tagging.h"

#include <audacious/plugin.h>
#include <audacious/output.h>
#include <libaudacious/util.h>
#include <libaudacious/titlestring.h>

#define MP4_VERSION	"ver.- 15 December 2004"
#define LIBMP4V2_VERSION "1.2.0"
#define MP4_ABOUT	"Written by ciberfred"
#define BUFFER_SIZE	FAAD_MIN_STREAMSIZE*64

const char *audmp4_id3_genres[GENRE_MAX] = {
    N_("Blues"), N_("Classic Rock"), N_("Country"), N_("Dance"),
    N_("Disco"), N_("Funk"), N_("Grunge"), N_("Hip-Hop"),
    N_("Jazz"), N_("Metal"), N_("New Age"), N_("Oldies"),
    N_("Other"), N_("Pop"), N_("R&B"), N_("Rap"), N_("Reggae"),
    N_("Rock"), N_("Techno"), N_("Industrial"), N_("Alternative"),
    N_("Ska"), N_("Death Metal"), N_("Pranks"), N_("Soundtrack"),
    N_("Euro-Techno"), N_("Ambient"), N_("Trip-Hop"), N_("Vocal"),
    N_("Jazz+Funk"), N_("Fusion"), N_("Trance"), N_("Classical"),
    N_("Instrumental"), N_("Acid"), N_("House"), N_("Game"),
    N_("Sound Clip"), N_("Gospel"), N_("Noise"), N_("AlternRock"),
    N_("Bass"), N_("Soul"), N_("Punk"), N_("Space"),
    N_("Meditative"), N_("Instrumental Pop"),
    N_("Instrumental Rock"), N_("Ethnic"), N_("Gothic"),
    N_("Darkwave"), N_("Techno-Industrial"), N_("Electronic"),
    N_("Pop-Folk"), N_("Eurodance"), N_("Dream"),
    N_("Southern Rock"), N_("Comedy"), N_("Cult"),
    N_("Gangsta Rap"), N_("Top 40"), N_("Christian Rap"),
    N_("Pop/Funk"), N_("Jungle"), N_("Native American"),
    N_("Cabaret"), N_("New Wave"), N_("Psychedelic"), N_("Rave"),
    N_("Showtunes"), N_("Trailer"), N_("Lo-Fi"), N_("Tribal"),
    N_("Acid Punk"), N_("Acid Jazz"), N_("Polka"), N_("Retro"),
    N_("Musical"), N_("Rock & Roll"), N_("Hard Rock"), N_("Folk"),
    N_("Folk/Rock"), N_("National Folk"), N_("Swing"),
    N_("Fast-Fusion"), N_("Bebob"), N_("Latin"), N_("Revival"),
    N_("Celtic"), N_("Bluegrass"), N_("Avantgarde"),
    N_("Gothic Rock"), N_("Progressive Rock"),
    N_("Psychedelic Rock"), N_("Symphonic Rock"), N_("Slow Rock"),
    N_("Big Band"), N_("Chorus"), N_("Easy Listening"),
    N_("Acoustic"), N_("Humour"), N_("Speech"), N_("Chanson"),
    N_("Opera"), N_("Chamber Music"), N_("Sonata"), N_("Symphony"),
    N_("Booty Bass"), N_("Primus"), N_("Porn Groove"),
    N_("Satire"), N_("Slow Jam"), N_("Club"), N_("Tango"),
    N_("Samba"), N_("Folklore"), N_("Ballad"), N_("Power Ballad"),
    N_("Rhythmic Soul"), N_("Freestyle"), N_("Duet"),
    N_("Punk Rock"), N_("Drum Solo"), N_("A Cappella"),
    N_("Euro-House"), N_("Dance Hall"), N_("Goa"),
    N_("Drum & Bass"), N_("Club-House"), N_("Hardcore"),
    N_("Terror"), N_("Indie"), N_("BritPop"), N_("Negerpunk"),
    N_("Polsk Punk"), N_("Beat"), N_("Christian Gangsta Rap"),
    N_("Heavy Metal"), N_("Black Metal"), N_("Crossover"),
    N_("Contemporary Christian"), N_("Christian Rock"),
    N_("Merengue"), N_("Salsa"), N_("Thrash Metal"),
    N_("Anime"), N_("JPop"), N_("Synthpop")
};


static void	mp4_init(void);
static void	mp4_about(void);
static void	mp4_play(char *);
static void	mp4_stop(void);
static void	mp4_pause(short);
static void	mp4_seek(int);
static int	mp4_getTime(void);
static void	mp4_cleanup(void);
static int	mp4_isFile(char *);
static void	mp4_getSongTitle(char *filename, char **, int *);
static void*	mp4Decode(void *);

void     audmp4_file_info_box(gchar *);

InputPlugin mp4_ip =
  {
    0,	// handle
    0,	// filename
    "MP4 Audio Plugin",
    mp4_init,
    mp4_about,
    0,	// configuration
    mp4_isFile,
    0,	//scandir
    mp4_play,
    mp4_stop,
    mp4_pause,
    mp4_seek,
    0,	// set equalizer
    mp4_getTime,
    0,	// get volume
    0,
    mp4_cleanup,
    0,	// obsolete
    0,	// send visualisation data
    0,	// set player window info
    0,	// set song title text
    mp4_getSongTitle,	// get song title text
    audmp4_file_info_box, // info box
    0,	// to output plugin
  };

typedef struct  _mp4cfg{
  gshort        file_type;
#define FILE_UNKNOW     0
#define FILE_MP4        1
#define FILE_AAC        2
}               Mp4Config;

static Mp4Config	mp4cfg;
static gboolean		bPlaying = FALSE;
static GThread		*decodeThread;
GStaticMutex 		mutex = G_STATIC_MUTEX_INIT;
static int		seekPosition = -1;

void getMP4info(char*);
int getAACTrack(MP4FileHandle);

/*
 * Function extname (filename)
 *
 *    Return pointer within filename to its extenstion, or NULL if
 *    filename has no extension.
 *
 */
static gchar *
extname(const char *filename)
{   
    gchar *ext = strrchr(filename, '.');

    if (ext != NULL)
        ++ext;

    return ext;
}

InputPlugin *get_iplugin_info(void)
{
  return(&mp4_ip);
}

static void mp4_init(void)
{
  mp4cfg.file_type = FILE_UNKNOW;
  seekPosition = -1;
  return;
}

static void mp4_play(char *filename)
{
  bPlaying = TRUE;
  decodeThread = g_thread_create((GThreadFunc)mp4Decode, g_strdup(filename), TRUE, NULL);
  return;
}

static void mp4_stop(void)
{
  if(bPlaying){
    bPlaying = FALSE;
    g_thread_join(decodeThread);
    mp4_ip.output->close_audio();
  }
}

static int	mp4_isFile(char *filename)
{
  if(filename){
    gchar*	extention;

    extention = strrchr(filename, '.');
    if (extention &&(
	!strcasecmp(extention, ".mp4") ||	// official extention
	!strcasecmp(extention, ".m4a") ||	// Apple mp4 extention
	!strcasecmp(extention, ".aac")		// old MPEG2/4-AAC extention
	)){
      return (1);
    }
  }
  return(0);
}

static void	mp4_about(void)
{
  static GtkWidget *aboutbox;

  if(aboutbox!=NULL)
    return;
  aboutbox = xmms_show_message("About MP4 AAC player plugin",
			       "libfaad2-" FAAD2_VERSION "\n"
			       "libmp4v2-" LIBMP4V2_VERSION "\n"
			       "plugin version: " MP4_VERSION "\n"
			       MP4_ABOUT,
			       "Ok", FALSE, NULL, NULL);
  gtk_signal_connect(GTK_OBJECT(aboutbox), "destroy",
                     GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                     &aboutbox);
}

static void	mp4_pause(short flag)
{
  mp4_ip.output->pause(flag);
}

static void	mp4_seek(int time)
{
  seekPosition = time;
  while(bPlaying && seekPosition!=-1)
    xmms_usleep(10000);
}

static int	mp4_getTime(void)
{
  if(!bPlaying)
    return (-1);
  else
    return (mp4_ip.output->output_time());
}

static void	mp4_cleanup(void)
{
}

#if 0
static void	mp4_getSongInfo(char *filename)
{
  if(mp4cfg.file_type == FILE_MP4)
    getMP4info(filename);
  else if(mp4cfg.file_type == FILE_AAC)
    ;
}
#endif

static gchar   *mp4_get_song_title(char *filename)
{
	MP4FileHandle mp4file;
	gchar *title = NULL;

	if (!(mp4file = MP4Read(filename, 0))) {
		MP4Close(mp4file);
	} else {
		TitleInput *input;
		gchar *tmpval;

		input = bmp_title_input_new();

		MP4GetMetadataName(mp4file, &input->track_name);
		MP4GetMetadataAlbum(mp4file, &input->album_name);
		MP4GetMetadataArtist(mp4file, &input->performer);
		MP4GetMetadataYear(mp4file, &tmpval);
		MP4GetMetadataGenre(mp4file, &input->genre);

		if (tmpval)
		{
			input->year = atoi(tmpval);
			free(tmpval);
		}

		input->file_name = g_path_get_basename(filename);
		input->file_path = g_path_get_dirname(filename);
		input->file_ext = extname(filename);

		title = xmms_get_titlestring(xmms_get_gentitle_format(), input);

		free (input->track_name);
		free (input->album_name);
		free (input->performer);
		free (input->genre);
		free (input->file_name);
		free (input->file_path);
		free (input);
	}

	if (!title)
	{
		title = g_path_get_basename(filename);
		if (extname(title))
			*(extname(title) - 1) = '\0';
	}

	return title;
}

static void	mp4_getSongTitle(char *filename, char **title_real, int *len_real)
{
	(*title_real) = mp4_get_song_title(filename);
	(*len_real) = -1;
}

static void *mp4Decode(void *args)
{
  MP4FileHandle mp4file;

  g_static_mutex_lock(&mutex);
  seekPosition = -1;
  bPlaying = TRUE;
  if(!(mp4file = MP4Read(args, 0))){
    mp4cfg.file_type = FILE_AAC;
    MP4Close(mp4file);
  }else{
    mp4cfg.file_type = FILE_MP4;
  }

  if(mp4cfg.file_type == FILE_MP4){
    // We are reading a MP4 file
    gint		mp4track;

    if((mp4track = getAACTrack(mp4file)) < 0){
      //TODO: check here for others Audio format.....
      g_print("Unsupported Audio track type\n");
      g_free(args);
      MP4Close(mp4file);
      bPlaying = FALSE;
      g_static_mutex_unlock(&mutex);
      g_thread_exit(NULL);
      return(NULL);
    }else{
      faacDecHandle	decoder;
      unsigned char	*buffer	= NULL;
      guint		bufferSize = 0;
      gulong		samplerate;
      guchar		channels;
      guint		avgBitrate;
      MP4Duration	duration;
      gulong		msDuration;
      MP4SampleId	numSamples;
      MP4SampleId	sampleID = 1;
      gchar            *title;

      decoder = faacDecOpen();
      MP4GetTrackESConfiguration(mp4file, mp4track, &buffer, &bufferSize);
      if(!buffer)
	goto out;
      if(faacDecInit2(decoder, buffer, bufferSize, &samplerate, &channels)<0)
	goto out;
      g_free(buffer);
      if(channels == 0){
	g_print("Number of Channels not supported\n");
	goto out;
      }
      duration = MP4GetTrackDuration(mp4file, mp4track);
      msDuration = MP4ConvertFromTrackDuration(mp4file, mp4track, duration,
					       MP4_MSECS_TIME_SCALE);
      numSamples = MP4GetTrackNumberOfSamples(mp4file, mp4track);
      samplerate = MP4GetTrackTimeScale(mp4file, mp4track);
      mp4_ip.output->open_audio(FMT_S16_NE, samplerate, channels);
      mp4_ip.output->flush(0);
      title = mp4_get_song_title(args);
      avgBitrate = MP4GetTrackBitRate(mp4file, mp4track);
      mp4_ip.set_info(title, msDuration, avgBitrate, samplerate, channels);

      while(bPlaying){
	void*			sampleBuffer;
	faacDecFrameInfo	frameInfo;    
	gint			rc;

	if(seekPosition!=-1){
	  duration = MP4ConvertToTrackDuration(mp4file,
					       mp4track,
					       seekPosition*1000,
					       MP4_MSECS_TIME_SCALE);
	  sampleID = MP4GetSampleIdFromTime(mp4file, mp4track, duration, 0);
	  mp4_ip.output->flush(seekPosition*1000);
	  seekPosition = -1;
	}
	buffer=NULL;
	bufferSize=0;
	if(sampleID > numSamples){
	  goto out;
	}
	rc = MP4ReadSample(mp4file, mp4track, sampleID++, &buffer, &bufferSize,
			   NULL, NULL, NULL, NULL);
	if((rc==0) || (buffer== NULL)){
	  g_print("MP4: read error\n");
	  sampleBuffer = NULL;
	  sampleID=0;
	  mp4_ip.output->buffer_free();
	  goto out;
	}else{
	  sampleBuffer = faacDecDecode(decoder, &frameInfo, buffer, bufferSize);
	  if(frameInfo.error > 0){
	    g_print("MP4: %s\n",
		    faacDecGetErrorMessage(frameInfo.error));
	    goto out;
	  }
	  if(buffer){
	    g_free(buffer); buffer=NULL; bufferSize=0;
	  }
	  while(bPlaying && mp4_ip.output->buffer_free()<frameInfo.samples<<1)
	    xmms_usleep(30000);
	}
	produce_audio(mp4_ip.output->written_time(),
		   FMT_S16_NE,
		   channels,
		   frameInfo.samples<<1,
		   sampleBuffer, &bPlaying);
      }
      while(bPlaying && mp4_ip.output->buffer_free()){
	xmms_usleep(10000);
      }
out:
      mp4_ip.output->close_audio();
      g_free(args);
      faacDecClose(decoder);
      MP4Close(mp4file);
      bPlaying = FALSE;
      g_static_mutex_unlock(&mutex);
      g_thread_exit(NULL);
      return(NULL);
    }
  } else{
    // WE ARE READING AN AAC FILE
    FILE		*file = NULL;
    faacDecHandle	decoder = 0;
    guchar		*buffer = 0;
    gulong		bufferconsumed = 0;
    gulong		samplerate = 0;
    guchar		channels;
    gulong		buffervalid = 0;
    TitleInput*		input;
    gchar		*temp = g_strdup(args);
    gchar		*ext  = strrchr(temp, '.');
    gchar		*xmmstitle = NULL;
    faacDecConfigurationPtr config;

    if((file = fopen(args, "rb")) == 0){
      g_print("AAC: can't find file %s\n", (char*)args);
      bPlaying = FALSE;
      g_static_mutex_unlock(&mutex);
      g_thread_exit(NULL);
    }
    if((decoder = faacDecOpen()) == NULL){
      g_print("AAC: Open Decoder Error\n");
      fclose(file);
      bPlaying = FALSE;
      g_static_mutex_unlock(&mutex);
      g_thread_exit(NULL);
    }
    config = faacDecGetCurrentConfiguration(decoder);
    config->useOldADTSFormat = 0;
    faacDecSetConfiguration(decoder, config);
    if((buffer = g_malloc(BUFFER_SIZE)) == NULL){
      g_print("AAC: error g_malloc\n");
      fclose(file);
      bPlaying = FALSE;
      faacDecClose(decoder);
      g_static_mutex_unlock(&mutex);
      g_thread_exit(NULL);
    }
    if((buffervalid = fread(buffer, 1, BUFFER_SIZE, file))==0){
      g_print("AAC: Error reading file\n");
      g_free(buffer);
      fclose(file);
      bPlaying = FALSE;
      faacDecClose(decoder);
      g_static_mutex_unlock(&mutex);
      g_thread_exit(NULL);
    }
    XMMS_NEW_TITLEINPUT(input);
    input->file_name = (char*)g_basename(temp);
    input->file_ext = ext ? ext+1 : NULL;
    input->file_path = temp;
    if(!g_ascii_strncasecmp((gchar*)buffer, "ID3", 3)){
      gint size = 0;

      fseek(file, 0, SEEK_SET);
      size = (buffer[6]<<21) | (buffer[7]<<14) | (buffer[8]<<7) | buffer[9];
      size+=10;
      fread(buffer, 1, size, file);
      buffervalid = fread(buffer, 1, BUFFER_SIZE, file);
    }
    xmmstitle = xmms_get_titlestring(xmms_get_gentitle_format(), input);
    if(xmmstitle == NULL)
      xmmstitle = g_path_get_basename(input->file_name);
    if(temp) g_free(temp);
    if(input->performer) g_free(input->performer);
    if(input->album_name) g_free(input->album_name);
    if(input->track_name) g_free(input->track_name);
    if(input->genre) g_free(input->genre);
    g_free(input);
    bufferconsumed = faacDecInit(decoder,
				 buffer,
				 buffervalid,
				 &samplerate,
				 &channels);
    if(mp4_ip.output->open_audio(FMT_S16_NE,samplerate,channels) == FALSE){
      g_print("AAC: Output Error\n");
      g_free(buffer); buffer=0;
      faacDecClose(decoder);
      fclose(file);
      mp4_ip.output->close_audio();
      /*
      if(positionTable){
	g_free(positionTable); positionTable=0;
      }
      */
      g_free(xmmstitle);
      bPlaying = FALSE;
      g_static_mutex_unlock(&mutex);
      g_thread_exit(NULL);
    }
    //if(bSeek){
    //mp4_ip.set_info(xmmstitle, lenght*1000, -1, samplerate, channels);
      //}else{
    mp4_ip.set_info(xmmstitle, -1, -1, samplerate, channels);
      //}
    mp4_ip.output->flush(0);

    while(bPlaying && buffervalid > 0){
      faacDecFrameInfo	finfo;
      unsigned long	samplesdecoded;
      char*		sample_buffer = NULL;
      /*
	if(bSeek && seekPosition!=-1){
	fseek(file, positionTable[seekPosition], SEEK_SET);
	bufferconsumed=0;
	buffervalid = fread(buffer, 1, BUFFER_SIZE, file);
	aac_ip.output->flush(seekPosition*1000);
	seekPosition=-1;
	}
      */
      if(bufferconsumed > 0){
	memmove(buffer, &buffer[bufferconsumed], buffervalid-bufferconsumed);
	buffervalid -= bufferconsumed;
	buffervalid += fread(&buffer[buffervalid], 1,
			     BUFFER_SIZE-buffervalid, file);
	bufferconsumed = 0;
      }
      sample_buffer = faacDecDecode(decoder, &finfo, buffer, buffervalid);
      if(finfo.error){
	config = faacDecGetCurrentConfiguration(decoder);
	if(config->useOldADTSFormat != 1){
	  faacDecClose(decoder);
	  decoder = faacDecOpen();
	  config = faacDecGetCurrentConfiguration(decoder);
	  config->useOldADTSFormat = 1;
	  faacDecSetConfiguration(decoder, config);
	  finfo.bytesconsumed=0;
	  finfo.samples = 0;
	  faacDecInit(decoder,
		      buffer,
		      buffervalid,
		      &samplerate,
		      &channels);
	}else{
	  g_print("FAAD2 Warning %s\n", faacDecGetErrorMessage(finfo.error));
	  buffervalid = 0;
	}
      }
      bufferconsumed += finfo.bytesconsumed;
      samplesdecoded = finfo.samples;
      if((samplesdecoded<=0) && !sample_buffer){
	g_print("AAC: error sample decoding\n");
	continue;
      }
      while(bPlaying && mp4_ip.output->buffer_free() < (samplesdecoded<<1)){
	xmms_usleep(10000);
      }
      mp4_ip.add_vis_pcm(mp4_ip.output->written_time(),
			 FMT_S16_LE, channels,
			 samplesdecoded<<1, sample_buffer);
      mp4_ip.output->write_audio(sample_buffer, samplesdecoded<<1);
    }
    while(bPlaying && mp4_ip.output->buffer_playing()){
      xmms_usleep(10000);
    }
    mp4_ip.output->buffer_free();
    mp4_ip.output->close_audio();
    bPlaying = FALSE;
    g_free(buffer);
    faacDecClose(decoder);
    g_free(xmmstitle);
    fclose(file);
    seekPosition = -1;
    /*
    if(positionTable){
      g_free(positionTable); positionTable=0;
    }
    */
    bPlaying = FALSE;
    g_static_mutex_unlock(&mutex);
    g_thread_exit(NULL);
    return(NULL);    
  }
}
