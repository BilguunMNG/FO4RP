#ifndef __FOFMODSYSTEM__
#define __FOFMODSYSTEM__


#include "fofmodsystem.h"
#include "_defines.fos"
#include "fonline.h"
#include "tinydir.h"
#include "miniz.h"
#include "util.h"
#include "stdio.h"
#include "perfcounter.h"
#include <cstring>

namespace FOFMOD
{	
	const int fileExtNum = 4;
	const std::string fileExtensions[fileExtNum] = { std::string(".ogg"), std::string(".flac"), std::string(".wav"), std::string(".mp3") };

	const char* SoundWorkDir = ".\\data\\sounds\\sfx\\";
	const char* MusicWorkDir = ".\\data\\sound\\music\\";

	bool IsValidSoundExtension( const char* filename )
	{
		bool result = false;
		if( filename )
		{
			for( int i = 0; i < fileExtNum; i ++ )
			{
				if( cstr_ends_with( filename, fileExtensions[i].c_str() ) )
				{
					result = true;
					break;
				}
			}	
		}
		return result;
	}

	System::CacheSoundData::CacheSoundData() // BANNED
	{
		// BANNED
		// BANNED
		// BANNED
	}

	System::CacheSoundData::CacheSoundData(FOFMOD::System* ownerSystem )
	{
		this->refcount = 0;
		this->data  = NULL;
		this->size  = 0;
		this->owner = ownerSystem;
	}

	System::CacheSoundData::~CacheSoundData()
	{
		if( this->data )
		{
			free ( data );
		}

		if( this->owner )
		{
			FOFMOD_DEBUG_LOG("Deletincg cached data \n")
			this->owner->DeleteCachedSound( this->it );
		}
	}

	void System::CacheSoundData::Addref()
	{
		#if defined ( FO_GCC )
		INTERLOCKED_INCREMENT (&this->refcount, 1);
		#else
		INTERLOCKED_INCREMENT (&this->refcount );
		#endif
	}

	void System::CacheSoundData::Release()
	{
		if(!
		#if defined ( FO_GCC ) 
		INTERLOCKED_DECREMENT ( &this->refcount, 1 )
		#else
		INTERLOCKED_DECREMENT ( &this->refcount )
		#endif
		)
		{
			FOFMOD_DEBUG_LOG("Cache sound data refcount zero release \n");
			delete this;
		}

	}



	System::ChannelCallbackData::ChannelCallbackData()
	{
		this->system    = NULL;
		this->sound     = NULL;
		this->cacheData = NULL;
	}

	System::ChannelCallbackData::~ChannelCallbackData()
	{
		
	}

	// fmod supports single callback for each event, event should be determined inside the callback itself
	FMOD_RESULT F_CALLBACK System::ChannelCallback( 	FMOD_CHANNELCONTROL *channelcontrol, 
														FMOD_CHANNELCONTROL_TYPE controlType, 
														FMOD_CHANNELCONTROL_CALLBACK_TYPE callbacktype, 
														void* commanddata1, 
														void* commanddata2 )
	{

		FOFMOD_DEBUG_LOG("Channel callback \n");
		if( callbacktype  == FMOD_CHANNELCONTROL_CALLBACK_TYPE::FMOD_CHANNELCONTROL_CALLBACK_END )
		{
			FOFMOD_DEBUG_LOG("Channel sound end callback \n");
			FMOD::Channel* channel = (FMOD::Channel*)channelcontrol;
			FOFMOD::System::ChannelCallbackData* cbdata = NULL; 
			channel->getUserData( (void**)&cbdata );
			if( cbdata )
			{
				FOFMOD::Sound* snd = cbdata->sound;
				snd->Release();
				FOFMOD::System::CacheSoundData* cacheData = cbdata->cacheData;

				if( cacheData )
				{
					cacheData->Release();
				}

				delete cbdata;
				FOFMOD_DEBUG_LOG("Channel <%d> play end \n");
			}
		}
		
		return FMOD_OK;
	}

	System::System()
	{
		this->initialized = false;
		this->FSystem = NULL;
		this->soundChannelGroup = NULL;
		this->musicChannelGroup = NULL;
	}

	System::~System()
	{
		if( this->soundChannelGroup )
		{
			this->soundChannelGroup = NULL;
		}

		if( this->musicChannelGroup )
		{
			this->musicChannelGroup = NULL;
		}

		if( this->FSystem )
		{
			this->FSystem->release();
			this->FSystem = NULL;
		}

		this->indexedArchives.clear();
		this->initialized = false;
	}

	void System::Update()
	{
		this->FSystem->update();
	}

	FMOD_RESULT System::Initialize( unsigned int channelCount )
	{
		FMOD_RESULT result;
		if( !this->IsInit() )
		{
			result = FMOD::System_Create(&this->FSystem);
			if (result != FMOD_OK)
			{
			    Log("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
			}
			else
			{
				unsigned int version;
				result = this->FSystem->getVersion(&version);

			    if (version < FMOD_VERSION)
			    {
			        Log("FMOD lib version %08x doesn't match header version %08x", version, FMOD_VERSION);
			    }
			    else
			    {
			    	// continue initialize
			    	FOFMOD_DEBUG_LOG("FMOD version %u \n", version);
					result = this->FSystem->init( channelCount, FMOD_INIT_NORMAL, 0 );
					if (result != FMOD_OK)
					{
					    Log("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
					}
					else
					{
						// continue initialize
						// channel groups
						result = this->FSystem->createChannelGroup( "sounds", &this->soundChannelGroup );
						if( result == FMOD_OK )
						{
							result = this->FSystem->createChannelGroup( "music", &this->soundChannelGroup );

							if( result == FMOD_OK )
							{
								#ifdef FOFMOD_DEBUG
								FOFMOD_DEBUG_LOG("Codec plugins listing... \n");
								int pluginsCount = 0;
								result = this->FSystem->getNumPlugins( FMOD_PLUGINTYPE_CODEC, &pluginsCount );

								for( int i = 0; i < pluginsCount; i ++ )
								{
									unsigned int pluginHandle = 0;
									FMOD_RESULT plugResult = this->FSystem->getPluginHandle ( FMOD_PLUGINTYPE_CODEC, i, &pluginHandle );
									if( plugResult = FMOD_OK )
									{
										const int nameLen = 1024;
										char* pluginName = new char[nameLen];
										unsigned int version = 0;
										FMOD_PLUGINTYPE ptype;
										plugResult = this->FSystem->getPluginInfo( pluginHandle, &ptype, pluginName, nameLen, &version );
										if( plugResult )
										{
											FOFMOD_DEBUG_LOG("Codec plugin loaded %s %d \n", pluginName, version );
										}
										delete pluginName;
									}
								}

								#endif // FOFMOD_DEBUG
							}
						}
					}
			    }
			}
		}
		else
			result = FMOD_OK;
		
		if( result == FMOD_OK )
			this->initialized = true;

		return result;
	}

	bool System::TouchArchive( const std::string& filename )
	{

		FOFMOD_DEBUG_LOG("Touch archive file at path <%s>. \n", filename.c_str() );
		bool result = false;

		char ext[12];
		cstr_path_get_ext( filename.c_str(), (char*)&ext, 12 );
		_strlwr( (char*)&ext );
		if( strcmp( ext, ".zip" ) == 0 )
		{
			FOFMOD_DEBUG_LOG("Its a fuggen zip \n" );
			FOFMOD::ZipFile* zipFile = new FOFMOD::ZipFile();
			zipFile->Open( filename.c_str() );
			if( zipFile->IsOpened() )
			{
				// file exist and opened
				zipFile->Touch();
				zipFile->Close();
				this->indexedArchives.push_back( zipFile );
				result = true;
			}
			else
			{
				FOFMOD_DEBUG_LOG("Touch archive file at path <%s> failed. \n", filename.c_str() );
				delete zipFile;
			}
		}

		return result;
	}

	void System::DeleteCachedSound( CachedDataMap::iterator iterator )
	{
		// mother fucker
		this->cachedSoundsData.erase( iterator );
	}


	void System::Play( const std::string& soundName, FMOD::ChannelGroup* group, FOFMOD::Channel** chn, bool paused )
	{
		#ifdef FOFMOD_DEBUG
			#ifdef __PERFCOUNT__
			static FOFMOD::Perfcounter perfcounter;
			perfcounter.Start();
			#endif // __PERFCOUNT__
		#endif // FOFMOD_DEBUG

		FOFMOD::Sound* snd   =  NULL;
		CacheSoundData* cacheData = NULL;
		this->GetSound( soundName, &snd, &cacheData );

		if( snd )
		{
			if( cacheData )
			{
				// addref for play
				cacheData->Addref();
			}

			(*chn) = new FOFMOD::Channel();
			FMOD_RESULT result = this->FSystem->playSound( snd->handle, group, true, &( (*chn)->handle) );
			if( result == FMOD_OK )
			{
				FOFMOD::System::ChannelCallbackData* cbdata = new FOFMOD::System::ChannelCallbackData();
				cbdata->system = this;
				cbdata->sound  = snd;

				if( cacheData )
				{
					cbdata->cacheData = cacheData;
				}

				snd->Addref(); // we're referencing it for callback, release there, system sound release through FOFMOD sound release
				(*chn)->handle->setCallback( FOFMOD::System::ChannelCallback );
				(*chn)->handle->setUserData( cbdata );

				if( !paused )
					(*chn)->handle->setPaused( false );
			}
			else
			{
				if( cacheData )
				{
					cacheData->Release();
				}

				delete *chn;
				*chn = NULL;
			}
		}

		#ifdef FOFMOD_DEBUG
			#ifdef __PERFCOUNT__
			FOFMOD_DEBUG_LOG("PlaySound handle time <%f ms> sound <%s> \n ", perfcounter.Get(), soundName.c_str() );
			#endif // __PERFCOUNT__
		#endif // FOFMOD_DEBUG
	}


	FOFMOD::Channel* System::PlaySound( const std::string& soundName, bool paused )
	{	
		FOFMOD::Channel* chn = NULL;
		this->Play( soundName, this->soundChannelGroup, &chn, paused );
		return chn;
	}

	FOFMOD::Channel* System::PlayMusic( const std::string& soundName, bool paused )
	{

		FOFMOD::Channel* chn = NULL;
		this->Play( soundName, this->musicChannelGroup, &chn, paused );
		return chn;
	}

	void System::SoundFromMemory( void* ptr, unsigned int size, FOFMOD::Sound** sptr )
	{

		FMOD_CREATESOUNDEXINFO memLoadInfo = { sizeof(FMOD_CREATESOUNDEXINFO), size };

		FMOD::Sound* fsnd = NULL;
		FMOD_RESULT result = this->FSystem->createSound(  (const char*)(ptr), CREATEFLAGS_STREAM, &memLoadInfo, &fsnd );
		if( result == FMOD_OK )
		{
			*sptr = new FOFMOD::Sound();
			(*sptr)->handle = fsnd;
		}
	}

	void System::SoundFromArchive( const std::string& filename, FOFMOD::Sound** sptr, CacheSoundData** cache )
	{
		//check for archives for target file
		for( ArchiveFilePtrVec::iterator ari = this->indexedArchives.begin(); ari < this->indexedArchives.end(); ari++ )
		{
			unsigned int size = 0;
			void* binary = (*ari)->GetContent( filename.c_str(), &size );
			if( binary )
			{
				this->SoundFromMemory( binary, size, sptr );
				if( !( *sptr ) )
				{
					// could not create the sound
					free( binary );
				}
				else
				{
					this->AddCachedSound( filename, binary, size, cache );
				}
				break;
			}
		}
	}

	void System::SoundFromFile( const std::string& filename, FOFMOD::Sound** sptr, CacheSoundData** cache )
	{

		FMOD::Sound* fsnd = NULL;
		FILE* file = fopen( filename.c_str(), "rb" );
		if( file )
		{
			FOFMOD_DEBUG_LOG("File opened \n");

			fseek( file, 0, SEEK_END );
			unsigned int size = ftell( file );
			char* binary = (char*)malloc(size);
			rewind( file );

			unsigned int readSize = fread( binary, 1, size, file );
			fclose( file );
			if( readSize == size ) 
			{
				FOFMOD_DEBUG_LOG("Play sound from file read ok <%d> <%d> \n", size, readSize);

				this->SoundFromMemory( binary, size, sptr );

				if( !(*sptr) )
				{
					free ( binary );
				}
				else
				{
					this->AddCachedSound( filename, binary, size, cache );
				}		
			}
			else
			{
				// error reading file to heap
				free( binary );
			}
		}
		
	}

	void System::GetSound( const std::string& filename, FOFMOD::Sound** sptr, CacheSoundData** cache  )
	{
		this->GetCachedSound( filename, cache );
		if( *cache )
		{
			FOFMOD_DEBUG_LOG("Play sound from cached \n");
			this->SoundFromMemory( (*cache)->data, (*cache)->size, sptr );
		}
		else
		{
			FOFMOD_DEBUG_LOG("Play sound from file \n")
			this->SoundFromFile( filename, sptr, cache );

			if( !(*sptr) )
			{
				// check for archives
				FOFMOD_DEBUG_LOG("Sound from file failed, trying sound from archive. \n")
				this->SoundFromArchive( filename, sptr, cache );
			}
		}
	}

	void System::AddCachedSound( const std::string& filename, void* data, unsigned int size, CacheSoundData** cache )
	{
		this->GetCachedSound( filename, cache );
		if( !(*cache) )
		{
			*cache = new FOFMOD::System::CacheSoundData( this );
			(*cache)->data = data;
			(*cache)->size = size;
			std::pair< CachedDataMap::iterator, bool > insertionResult = this->cachedSoundsData.insert( std::pair< std::string , FOFMOD::System::CacheSoundData* >( filename, (*cache) ) );
			if( insertionResult.second )
			{
				(*cache)->it = insertionResult.first;
			}
			else
			{
				FOFMOD_DEBUG_LOG("Caching playing sound fail <%s> \n", filename.c_str() );
				delete (*cache);
				*cache = NULL;
			}
		}
		else
		{
			// already there
			FOFMOD_DEBUG_LOG("Caching playing sound already cached <%s> \n", filename.c_str() );
		}
	}

	void System::GetCachedSound( const std::string& filename, CacheSoundData** cache )
	{
		CachedDataMap::iterator found = this->cachedSoundsData.find( filename );
		if( found != this->cachedSoundsData.end () )
		{
			*cache = found->second;
		}
	}

	void System::SetMusicVolume( float volume )
	{
		if( this->musicChannelGroup )
		{
			this->musicChannelGroup->setVolume( volume );
		}
	}

	void System::GetMusicVolume( float* volume )
	{
		if( this->musicChannelGroup )
		{
			this->musicChannelGroup->getVolume( volume );
		}
	}

	void System::SetSoundsVolume( float volume )
	{
		if( this->soundChannelGroup )
		{
			this->soundChannelGroup->setVolume( volume );
		}
	}

	void System::GetSoundsVolume( float* volume )
	{
		if( this->soundChannelGroup )
		{
			this->soundChannelGroup->getVolume( volume );
		}
	}

	void System::PauseAllSounds( bool state )
	{
		if( this->soundChannelGroup )
		{
			this->soundChannelGroup->setPaused( state );
		}
	}

	void System::PauseAllMusic( bool state )
	{
		if( this->musicChannelGroup )
		{
			this->musicChannelGroup->setPaused( state );
		}
	}


	void System::StopAllSounds()
	{
		if( this->soundChannelGroup )
		{
			this->soundChannelGroup->stop();
		}
	}
	
	void System::StopAllMusic()
	{
		if( this->musicChannelGroup )
		{
			this->musicChannelGroup->stop();
		}
	}

	void System::StopAll()
	{
		this->StopAllSounds();
		this->StopAllMusic();
	}

	void System::PauseAll( bool state )
	{
		this->PauseAllSounds( state );
		this->PauseAllMusic( state );
	}

	void System::Set3DSettings( float dopplerScale, float distanceFactor, float rolloffScale )
	{
		this->FSystem->set3DSettings( dopplerScale, distanceFactor, rolloffScale );
	}


	#define SETLISTENERDATA(data, x, y, z) \
	this->listener.data.x = x; \
	this->listener.data.y = y; \
	this->listener.data.z = z; 

	void System::Set3DListenerPosition( float x, float y, float z )
	{
		SETLISTENERDATA(position, x, y, z);
		this->FSystem->set3DListenerAttributes( 0, &(this->listener.position), NULL, NULL, NULL );
	}

	void System::Set3DListenerVelocity( float x, float y, float z )
	{
		SETLISTENERDATA(velocity, x, y, z);
		this->FSystem->set3DListenerAttributes( 0, NULL, &(this->listener.velocity), NULL, NULL );
	}

	void System::Set3DListenerForward( float x, float y, float z )
	{
		SETLISTENERDATA(forward, x, y, z);
		this->FSystem->set3DListenerAttributes( 0, NULL, NULL, &(this->listener.forward), NULL );
	}

	void System::Set3DListenerUp( float x, float y, float z )
	{ 
		SETLISTENERDATA(up, x, y, z);
		this->FSystem->set3DListenerAttributes( 0, NULL, NULL, NULL, &(this->listener.up) );
	}


	//////////////////////////////////////////////////
	/////////////ANGELSCRIPT INTERFACING//////////////
	/////////////ANGELSCRIPT INTERFACING//////////////
	/////////////ANGELSCRIPT INTERFACING//////////////
	/////////////ANGELSCRIPT INTERFACING//////////////
	//////////////////////////////////////////////////


	FOFMOD::System* Script_System_Factory()
	{
		return new FOFMOD::System();
	}

}


#endif // __FOFMODSYSTEM__