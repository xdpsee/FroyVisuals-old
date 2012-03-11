package com.starlon.froyvisuals;

import android.app.Activity;
import android.os.Bundle;
import android.os.CountDownTimer;
import android.os.Looper;
import android.os.AsyncTask;
import android.os.ParcelFileDescriptor;
import android.content.Context;
import android.content.ContentUris;
import android.content.res.Configuration;
import android.content.SharedPreferences;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.ViewGroup;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.MotionEvent;
import android.view.Display;
import android.view.Surface;
import android.view.View.OnTouchListener;
import android.view.View.OnClickListener;
import android.view.GestureDetector;
import android.view.GestureDetector.SimpleOnGestureListener;
import android.view.ViewConfiguration;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Typeface;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.media.AudioFormat;
import android.util.Log;
import android.util.TypedValue;
import android.net.Uri;
import android.database.Cursor;
import android.provider.MediaStore;
import java.util.Timer;
import java.util.TimerTask;
import java.util.List;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map.Entry;
import java.io.FileDescriptor;
import java.io.OutputStreamWriter;
import java.io.IOException;
import java.lang.Process;

public class FroyVisuals extends Activity implements OnClickListener
{
    private final static String TAG = "FroyVisuals/FroyVisualsActivity";
    private final static String PREFS = "FroyVisualsPrefs";
    private final static int ARTWIDTH = 100;
    private final static int ARTHEIGHT = 100;
    private static Settings mSettings;
    private AudioRecord mAudio;
    private MediaRecorder mRecorder;
    private boolean mMicActive = false;
    private int PCM_SIZE = 1024;
    private static int RECORDER_SAMPLERATE = 44100;
    private static int RECORDER_CHANNELS = AudioFormat.CHANNEL_IN_STEREO;
    private static int RECORDER_AUDIO_ENCODING = AudioFormat.ENCODING_PCM_16BIT;
    public boolean mDoMorph;
    public String mMorph;
    public String mInput;
    public String mActor;
    private float mSongChanged = 0;
    private String mSongAction;
    public String mSongCommand;
    public String mSongArtist;
    public String mSongAlbum;
    public String mSongTrack;
    public Bitmap mAlbumArt;
    public boolean mHasRoot = false;
    public HashMap<String, Bitmap> mAlbumMap = new HashMap<String, Bitmap>();

    static private String mDisplayText = "Please wait...";

    private static int SWIPE_MIN_DISTANCE = 120;
    private static int SWIPE_MAX_OFF_PATH = 250;
    private static int SWIPE_THRESHOLD_VELOCITY = 200;
    private GestureDetector gestureDetector;
    OnTouchListener gestureListener;

    private FroyVisualsView mView;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle state)
    {
        super.onCreate(state);

        mSettings = new Settings(this);

        this.requestWindowFeature(Window.FEATURE_NO_TITLE);

        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN);

        mView = new FroyVisualsView(this);

        // Don't dim screen
        mView.setKeepScreenOn(true);

        final ViewConfiguration vc = ViewConfiguration.get((Context)this);

        SWIPE_MIN_DISTANCE = vc.getScaledTouchSlop();
        SWIPE_THRESHOLD_VELOCITY = vc.getScaledMinimumFlingVelocity();
        SWIPE_MAX_OFF_PATH = vc.getScaledMaximumFlingVelocity();

        class MyGestureDetector extends SimpleOnGestureListener {
            @Override
            public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
                synchronized(mView.mBitmap)
                {
                    try {
                        if (Math.abs(e1.getY() - e2.getY()) > SWIPE_MAX_OFF_PATH)
                            return false;
                        if(e1.getX() - e2.getX() > SWIPE_MIN_DISTANCE && 
                            Math.abs(velocityX) > SWIPE_THRESHOLD_VELOCITY) {
                            Log.w(TAG, "Left swipe...");
                            mView.switchScene(1);
                            // Left swipe
                        }  else if (e2.getX() - e1.getX() > SWIPE_MIN_DISTANCE && 
                            Math.abs(velocityX) > SWIPE_THRESHOLD_VELOCITY) {
                            Log.w(TAG, "Right swipe...");
                            mView.switchScene(0);
                            // Right swipe
                        }
                    } catch (Exception e) {
                        Log.w(TAG, "Failure in onFling");
                        // nothing
                    }
                }
                return false;
            }
        }
        // Gesture detection
        gestureDetector = new GestureDetector(new MyGestureDetector());
        gestureListener = new View.OnTouchListener() {
            public boolean onTouch(View v, MotionEvent event) {
                return gestureDetector.onTouchEvent(event);
            }
        };
        mView.setOnClickListener(FroyVisuals.this);
        mView.setOnTouchListener(gestureListener);

        setContentView(mView);

        mHasRoot = checkRoot();

        enableMic();
    }

    public BroadcastReceiver mReceiver = new BroadcastReceiver() {
 
        @Override
        public void onReceive(Context context, Intent intent)
        {
            // intent.getAction() returns one of the following:
            // com.android.music.metachanged - new track has started
            // com.android.music.playstatechanged - playback queue has changed
            // com.android.music.playbackcomplete - playback has stopped, last file played
            // com.android.music.queuechanged - play-state has changed (pause/resume)
            String action = intent.getAction();

            if(action.equals("com.android.music.metachanged"))
            {
                mSongCommand = intent.getStringExtra("command");
                long id = intent.getLongExtra("id", -1);
                mSongArtist = intent.getStringExtra("artist");
                mSongAlbum = intent.getStringExtra("album");
                mSongTrack = intent.getStringExtra("track");
                mSongChanged = System.currentTimeMillis();
                mAlbumArt = mAlbumMap.get(mSongAlbum);
                NativeHelper.newSong();
                warn("(" + mSongTrack + ")", 5000, true);
            }
            else if(action.equals("com.android.music.playbackcomplete"))
            {
                mSongCommand = null;
                mSongArtist = null;
                mSongAlbum = null;
                mSongTrack = null;
                mSongChanged = 0;
                mAlbumArt = null;
                NativeHelper.newSong();
                warn("Ended playback...", true);
            }
        }
    };

    public void onClick(View v) {
/*
        Filter f = (Filter) v.getTag();
        FilterFullscreenActivity.show(this, input, f);
*/
    }

    public void updatePrefs()
    {

        SharedPreferences settings = getSharedPreferences(PREFS, 0);

        mDoMorph = settings.getBoolean("doMorph", true);

        NativeHelper.setMorphStyle(mDoMorph);

        mMorph = settings.getString("prefs_morph_selection", "alphablend");
        mInput = "mic";//settings.getString("prefs_input_selection", "alsa");
        mActor = settings.getString("prefs_actor_selection", "jakdaw");

        NativeHelper.morphSetCurrentByName(mMorph);
        NativeHelper.inputSetCurrentByName(mInput);
        NativeHelper.actorSetCurrentByName(mActor);
    }

    public void onResume()
    {
        super.onResume();

        updatePrefs();

        IntentFilter iF = new IntentFilter();
        iF.addAction("com.android.music.metachanged");
        iF.addAction("com.android.music.playstatechanged");
        iF.addAction("com.android.music.playbackcomplete");
        iF.addAction("com.android.music.queuechanged");

        registerReceiver(mReceiver, iF);

        mView.startThread();

        getAlbumArt();
    }

    public void onStop()
    {
        super.onStop();

        SharedPreferences settings = getSharedPreferences(PREFS, 0);
        SharedPreferences.Editor editor = settings.edit();

        int morph = NativeHelper.morphGetCurrent();
        int input = NativeHelper.inputGetCurrent();
        int actor = NativeHelper.actorGetCurrent();

        this.setMorph(NativeHelper.morphGetName(morph));
        this.setInput(NativeHelper.inputGetName(input));
        this.setActor(NativeHelper.actorGetName(morph));

        editor.putString("prefs_morph_selection", mMorph);
        editor.putString("prefs_input_selection", mInput);
        editor.putString("prefs_actor_selection", mActor);

        editor.putBoolean("doMorph", mDoMorph);

        //Commit edits
        editor.commit();

        unregisterReceiver(mReceiver);

        mView.stopThread();
        
        releaseAlbumArt();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.froyvisuals, menu);
        return true;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig)
    {
        super.onConfigurationChanged(newConfig);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item)
    {
        switch(item.getItemId())
        {
            case R.id.about:
            {
                startActivity(new Intent(this, AboutActivity.class));
                return true;
            }

            case R.id.settings:
            {
                startActivity(new Intent(this, PreferencesActivity.class));
                return true;
            }

/*
            case R.id.about_plugins:
            {
                startActivity(new Intent(this, AboutPluginsActivity.class));
                return true;
            }
*/
            case R.id.close_app:
            {
                NativeHelper.visualsQuit();
                return true;
            }
            case R.id.input_stub:
            {
                synchronized(mView.mBitmap)
                {
                    int index = NativeHelper.cycleInput(1);
    
                    String input = NativeHelper.inputGetName(index);
    
                    if(input == "mic")
                    {
                        if(!enableMic())
                            index = NativeHelper.cycleInput(1);
                    } else {
                        mMicActive = false;
                    }

                    if(input.equals("alsa") && !checkRoot())
                        index = NativeHelper.cycleInput(1);
    
                    warn(NativeHelper.inputGetLongName(index), true);
                }
            }

            default:
            {
                Log.w(TAG, "Unhandled menu-item. This is a bug!");
                break;
            }
        }
        return false;
    }
    /* load our native library */
    static {
        System.loadLibrary("visual");
        //System.loadLibrary("common");
        System.loadLibrary("main");
    }

    public boolean checkRoot()
    {
        try {
            
            Process exec = Runtime.getRuntime().exec(new String[]{"su"});
    
            final OutputStreamWriter out = new OutputStreamWriter(exec.getOutputStream());
            out.write("exit\n");
            out.flush();
            Log.i(TAG, "Superuser detected...");
            return true; 

        } catch (IOException e)
        {
            e.printStackTrace();
        }
        Log.i(TAG, "Root not detected...");
        return false;
    }

    /* Get the current morph plugin name */
    public String getMorph()
    {
        return mMorph;
    }

    /* Get the current input plugin name */
    public String getInput()
    {
        return mInput;
    }

    /* Get the current actor plugin name */
    public String getActor()
    {
        return mActor;
    }

    /* Set the current morph plugin name */
    public void setMorph(String morph)
    {
        mMorph = morph;
    }

    /* Set the current input plugin name */
    public void setInput(String input)
    {
        mInput = input;
    }

    /* Set the current actor plugin name */
    public void setActor(String actor)
    {
        mActor = actor;
    }


    /* Set whether to morph or not */
    public void setDoMorph(boolean doMorph)
    {
        mDoMorph = doMorph;
    }

    /* Get whether to morph or not */
    public boolean getDoMorph()
    {
        return mDoMorph;
    }

    /* Display a warning text: provide text, time in milliseconds, and priority */
    private long mLastRefresh = 0l;
    private int mLastDelay = 0;
    public boolean warn(String text, int millis, boolean priority)
    {
        long now = System.currentTimeMillis();

        if((now - mLastRefresh) < mLastDelay && !priority) 
            return false;

        mDisplayText = text;

        mLastRefresh = now;

        mLastDelay = millis;

        return true;
    }

    /* Display warning: provide text. */
    public boolean warn(String text)
    {
        return warn(text, 2000, false);
    }

    /* Display warning: provide text and priority */
    public boolean warn(String text, boolean priority)
    {
        return warn(text, 2000, priority);
    }

    public String getDisplayText()
    {
        return mDisplayText;
    }

    public final Uri sArtworkUri = Uri.parse("content://media/external/audio/albumart");

    private void releaseAlbumArt()
    {
        for(Entry<String, Bitmap> entry : mAlbumMap.entrySet())
        {
            entry.getValue().recycle();
        }
    }

    private void getAlbumArt()
    {
        ContentResolver contentResolver = this.getContentResolver();

        List<Long> result = new ArrayList<Long>();
        List<String> map = new ArrayList<String>();
        Cursor cursor = contentResolver.query(MediaStore.Audio.Media.getContentUri("external"), 
            new String[]{MediaStore.Audio.Media.ALBUM_ID}, null, null, null);
        Cursor albumCursor = contentResolver.query(MediaStore.Audio.Media.getContentUri("external"), 
            new String[]{MediaStore.Audio.Media.ALBUM}, null, null, null);
    
        if (cursor.moveToFirst() && albumCursor.moveToFirst())
        {
            do{
                long albumId = cursor.getLong(0);
                if (!result.contains(albumId))
                {
                    String album = albumCursor.getString(0);
                    result.add(albumId);
                    Bitmap bm = getAlbumArt(albumId);
                    if(bm != null && album != null)
                        mAlbumMap.put(album, bm);
                }
            } while (cursor.moveToNext() && albumCursor.moveToNext());
        }
    }

    /* http://stackoverflow.com/questions/6591087/most-robust-way-to-fetch-album-art-in-android*/
    public Bitmap getAlbumArt(long album_id) 
    {
        if(album_id == -1) 
            return null;

        Bitmap bm = null;
        try 
        {
            Uri uri = ContentUris.withAppendedId(sArtworkUri, album_id);

            ParcelFileDescriptor pfd = ((Context)this).getContentResolver()
                .openFileDescriptor(uri, "r");

            if (pfd != null) 
            {
                FileDescriptor fd = pfd.getFileDescriptor();
                bm = BitmapFactory.decodeFileDescriptor(fd);
            }
        } catch (Exception e) {
            // Do nothing
        }
        Bitmap scaled = null;
        if(bm != null)
        {
            scaled = Bitmap.createScaledBitmap(bm, ARTWIDTH, ARTHEIGHT, false);
            bm.recycle();
        }
        return scaled;
    }

    private boolean enableMic()
    {
        mAudio = findAudioRecord();
        if(mAudio != null)
        {
            NativeHelper.resizePCM(PCM_SIZE, RECORDER_SAMPLERATE, RECORDER_CHANNELS, RECORDER_AUDIO_ENCODING);

            new Thread(new Runnable() {
                public void run() {
                    mMicActive = true;
                    mAudio.startRecording();
                    while(mMicActive)
                    {
                        
                        short[] data = new short[PCM_SIZE];
                        mAudio.read(data, 0, PCM_SIZE);
                        NativeHelper.uploadAudio(data);
                    }
                    mAudio.stop();
                }
            }).start();
            return true;
        }
        return false;
    }

    private static int[] mSampleRates = new int[] { 48000, 44100, 22050, 11025, 8000 };
    public AudioRecord findAudioRecord() {
        for (int rate : mSampleRates) {
            for (short audioFormat : new short[] { AudioFormat.ENCODING_PCM_16BIT, AudioFormat.ENCODING_PCM_8BIT}) {
                for (short channelConfig : new short[] { AudioFormat.CHANNEL_IN_STEREO, AudioFormat.CHANNEL_IN_MONO}) {
                    try {
                        Log.d(TAG, "Attempting rate " + rate + "Hz, bits: " + audioFormat + ", channel: "
                                + channelConfig);
                        int bufferSize = AudioRecord.getMinBufferSize(rate, channelConfig, audioFormat);
    
                        if (bufferSize != AudioRecord.ERROR_BAD_VALUE) {
                            // check if we can instantiate and have a success
                            AudioRecord recorder = new AudioRecord(MediaRecorder.AudioSource.MIC, rate, channelConfig, audioFormat, bufferSize);
    
                            if (recorder.getState() == AudioRecord.STATE_INITIALIZED)
                            {
                                PCM_SIZE = bufferSize / 4;
                                RECORDER_SAMPLERATE = rate;
                                RECORDER_CHANNELS = channelConfig;
                                RECORDER_AUDIO_ENCODING = audioFormat;
                                Log.d(TAG, "Opened mic: " + rate + "Hz, bits: " + audioFormat + ", channel: " + channelConfig);
                                return recorder;
                            }
                        }
                    } catch (Exception e) {
                        Log.e(TAG, rate + " Exception, keep trying.",e);
                    }
                }
            }
        }
        return null;
    }


}


