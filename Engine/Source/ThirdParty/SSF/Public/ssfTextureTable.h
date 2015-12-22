#ifndef _TEXTURETABLE_H_
#define _TEXTURETABLE_H_
// 
// SSF file handler
// Copyright (c) 2014 Donya Labs AB, all rights reserved
// 

#include "ssfAttributeTypes.h"

#include "ssfChunkObject.h"

namespace ssf
	{
	class ssfTexture;
	class ssfScene;
	class ssfScene;
	class ssfChunkHeader;

	class ssfTextureTable : public ssfChunkObject
		{
		public:
			/// The textures directory where textures are stored
			ssfAttribute< ssfString > TexturesDirectory;

			/// The list of textures in the texture table
			vector<ssfCountedPointer<ssfTexture>> TextureList;

			/// ssfTextureTable object constructor
			ssfTextureTable();

			/// validate the scene object and sub-object recursively
			/// if validation failes, it will raise a std::exception with the validation error
			void Validate( const ssfScene &parent , const ssfScene &scene );

		private:
			/// private read and write functions from/to ssf file
			virtual bool Impl_ReadAttribute( const ssfAttributeHeader *attr , ssfBinaryInputStream *is );
			virtual bool Impl_ReadChildChunk( const string *type , ssfBinaryInputStream *is );
			virtual void Impl_SetupWriteAttributes();
			virtual void Impl_SetupWriteChildChunks();
			virtual void Impl_WriteAttributes( ssfBinaryOutputStream *s );
			virtual void Impl_WriteChildChunks( ssfBinaryOutputStream *s );

		protected:
			~ssfTextureTable();
			friend class ssfCountedPointer<ssfTextureTable>;
		};

	typedef ssfCountedPointer<ssfTextureTable> pssfTextureTable;

	};

#endif//_TEXTURETABLE_H_
