// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('function_sequence.js');
importScripts('function_parallel.js');
importScripts('util.js');

function Id3Parser(parent) {
  MetadataParser.apply(this, [parent]);
  this.verbose = true;
}

Id3Parser.parserType = 'id3';

Id3Parser.prototype = {__proto__: MetadataParser.prototype};

Id3Parser.prototype.urlFilter = /\.(mp3)$/i;

/**
 * Reads synchsafe integer.
 * 'SynchSafe' term is taken from id3 documentation.
 *
 * @param {ByteReader} reader - reader to use
 * @param {int} length - bytes to read
 * @return {int}
 */
Id3Parser.readSynchSafe_ = function(reader, length) {
  var rv = 0;

  switch (length) {
    case 4:
      rv = reader.readScalar(1, false) << 21;
    case 3:
      rv |= reader.readScalar(1, false) << 14;
    case 2:
      rv |= reader.readScalar(1, false) << 7;
    case 1:
      rv |= reader.readScalar(1, false);
  }

  return rv;
};

/**
 * Reads 3bytes integer.
 *
 * @param {ByteReader} reader - reader to use
 * @return {int}
 */
Id3Parser.readUInt24_ = function(reader) {
  return reader.readScalar(2, false) << 16 | reader.readScalar(1, false);
};

/**
 * Reads string from reader with specified encoding
 *
 * @param {ByteReader} reader reader to use
 * @param {int} encoding string encoding.
 * @param {int} size maximum string size. Actual result may be shorter.
 *
 */
Id3Parser.prototype.readString_ = function(reader, encoding, size) {
  switch (encoding) {
    case Id3Parser.v2.ENCODING.ISO_8859_1:
      return reader.readNullTerminatedString(size);
    case Id3Parser.v2.ENCODING.UTF_16BE:
    case Id3Parser.v2.ENCODING.UTF_16:
    case Id3Parser.v2.ENCODING.UTF_8:
    default: {
      // TODO: implement reading of unicode strings
      this.error('Reading of unicode strings in ID3 tags is unimplemented');
      return '';
    }
  }
};

/**
 * Reads text frame from reader.
 *
 * @param {ByteReader} reader reader to use
 * @param {int} majorVersion major id3 version to use
 * @param {Object} frame frame so store data at
 * @param {int} end frame end position in reader
 */
Id3Parser.prototype.readTextFrame_ = function(reader,
                                              majorVersion,
                                              frame,
                                              end) {
  frame.encoding = reader.readScalar(1, false, end);
  frame.value = this.readString_(reader, frame.encoding, end - reader.tell());
};

/**
 * Reads user defined text frame from reader.
 *
 * @param {ByteReader} reader reader to use
 * @param {int} majorVersion major id3 version to use
 * @param {Object} frame frame so store data at
 * @param {int} end frame end position in reader
 */
Id3Parser.prototype.readUserDefinedTextFrame_ = function(reader,
                                                         majorVersion,
                                                         frame,
                                                         end) {
  frame.encoding = reader.readScalar(1, false, end);

  frame.description = this.readString_(
      reader,
      frame.encoding,
      end - reader.tell());

  frame.value = this.readString_(
      reader,
      frame.encoding,
      end - reader.tell());
};

Id3Parser.prototype.readPIC_ = function(reader, majorVersion, frame, end) {
  frame.encoding = reader.readScalar(1, false, end);
  frame.format = reader.readNullTerminatedString(3, end - reader.tell());
  frame.pictureType = reader.readScalar(1, false, end);
  frame.description = this.readString_(reader,
                                       frame.encoding,
                                       end - reader.tell());


  if (frame.format == '-->') {
    frame.imageUrl = reader.readNullTerminatedString(end - reader.tell());
  } else {
    frame.imageUrl = reader.readImage(end - reader.tell());
  }
};

Id3Parser.prototype.readAPIC_ = function(reader, majorVersion, frame, end) {
  this.vlog('Extracting picture');
  frame.encoding = reader.readScalar(1, false, end);
  frame.mime = reader.readNullTerminatedString(end - reader.tell());
  frame.pictureType = reader.readScalar(1, false, end);
  frame.description = this.readString_(
      reader,
      frame.encoding,
      end - reader.tell());

  if (frame.mime == '-->') {
    frame.imageUrl = reader.readNullTerminatedString(end - reader.tell());
  } else {
    frame.imageUrl = reader.readImage(end - reader.tell());
  }
};

/**
 * Reads string from reader with specified encoding
 *
 * @param {ByteReader} reader  reader to use
 * @return {Object} frame read
 */
Id3Parser.prototype.readFrame_ = function(reader, majorVersion) {
  var frame = {};

  reader.pushSeek(reader.tell(), ByteReader.SEEK_BEG);

  var position = reader.tell();

  frame.name = (majorVersion == 2)
      ? reader.readNullTerminatedString(3)
      : reader.readNullTerminatedString(4);

  if (frame.name == '')
    return null;

  this.vlog('Found frame ' + (frame.name) + ' at position ' + position );

  switch (majorVersion) {
    case 2:
      frame.size = Id3Parser.readUInt24_(reader);
      frame.headerSize = 6;
      break;
    case 3:
      frame.size = reader.readScalar(4, false);
      frame.headerSize = 10;
      frame.flags = reader.readScalar(2, false);
      break;
    case 4:
      frame.size = Id3Parser.readSynchSafe_(reader, 4);
      frame.headerSize = 10;
      frame.flags = reader.readScalar(2, false);
      break;
  }

  this.vlog('Found frame [' + frame.name + '] with size ['+frame.size+']');

  if (Id3Parser.v2.HANDLERS[frame.name]) {
    Id3Parser.v2.HANDLERS[frame.name].call(
        this,
        reader,
        majorVersion,
        frame,
        reader.tell() + frame.size);
  } else if (frame.name.charAt(0) == 'T' || frame.name.charAt(0) == 'W') {
    this.readTextFrame_(
        reader,
        majorVersion,
        frame,
        reader.tell() + frame.size);
  }

  reader.popSeek();

  reader.seek(frame.size + frame.headerSize, ByteReader.SEEK_CUR);

  return frame;
};

Id3Parser.prototype.parse = function (file, callback, onError) {
  var metadata = {};

  var self = this;

  this.log('Starting id3 parser for ' + file.name);

  var id3v1Parser = new FunctionSequence(
      'id3v1parser',
      [
        /**
         * Reads last 128 bytes of file in bytebuffer,
         * which passes further.
         * In last 128 bytes should be placed ID3v1 tag if available.
         * @param file - file which bytes to read.
         */
        function readTail(file) {
          util.readFileBytes(file, file.size - 128, file.size,
              this.nextStep, this.onError, this);
        },

        /**
         * Attempts to extract ID3v1 tag from 128 bytes long ByteBuffer
         * @param file file which tags are being extracted.
         *        Could be used for logging purposes.
         * @param {ByteReader} reader ByteReader of 128 bytes.
         */
        function extractId3v1(file, reader) {
          if ( reader.readString(3) == 'TAG') {
            this.logger.vlog('id3v1 found');
            var id3v1 = metadata.id3v1 = {};

            var title = reader.readNullTerminatedString(30).trim();

            if (title.length > 0) {
              id3v1.title = title;
            }

            reader.seek(3 + 30, ByteReader.SEEK_BEG);

            var artist = reader.readNullTerminatedString(30).trim();
            if (artist.length > 0) {
              id3v1.artist = artist;
            }

            reader.seek(3 + 30 + 30, ByteReader.SEEK_BEG);

            var album = reader.readNullTerminatedString(30).trim();
            if (album.length > 0) {
              id3v1.album = album;
            }
          }
          this.nextStep();
        }
      ],
      this
  );

  var id3v2Parser = new FunctionSequence(
      'id3v2parser',
      [
        function readHead(file) {
          util.readFileBytes(file, 0, 10, this.nextStep, this.onError,
              this);
        },

        /**
         * Check if passed array of 10 bytes contains ID3 header.
         * @param file to check and continue reading if ID3 metadata found
         * @param {ByteReader} reader reader to fill with stream bytes.
         */
        function checkId3v2(file, reader) {
          if (reader.readString(3) == 'ID3') {
            this.logger.vlog('id3v2 found');
            var id3v2 = metadata.id3v2 = {};
            id3v2.major = reader.readScalar(1, false);
            id3v2.minor = reader.readScalar(1, false);
            id3v2.flags = reader.readScalar(1, false);
            id3v2.size = Id3Parser.readSynchSafe_(reader, 4);

            util.readFileBytes(file, 10, 10 + id3v2.size, this.nextStep,
                this.onError, this);
          } else {
            this.finish();
          }
        },

        /**
         * Extracts all ID3v2 frames from given bytebuffer.
         * @param file being parsed.
         * @param {ByteReader} reader to use for metadata extraction.
         */
        function extractFrames(file, reader) {
          var id3v2 = metadata.id3v2;

          if ((id3v2.major > 2)
              && (id3v2.flags & Id3Parser.v2.FLAG_EXTENDED_HEADER != 0)) {
            // Skip extended header if found
            if (id3v2.major == 3) {
              reader.seek(reader.readScalar(4, false) - 4);
            } else if (id3v2.major == 4) {
              reader.seek(Id3Parser.readSynchSafe_(reader, 4) - 4);
            }
          }

          var frame;

          while (frame = self.readFrame_(reader, id3v2.major)) {
            metadata.id3v2[frame.name] = frame;
          }

          this.nextStep();
        },

        /**
         * Adds 'description' object to metadata.
         * 'description' used to unify different parsers and make
         * metadata parser-aware.
         * Description is array if value-type pairs. Type should be used
         * to properly format value before displaying to user.
         */
        function prepareDescription() {
          var id3v2 = metadata.id3v2;

          if (id3v2['APIC'])
            metadata.thumbnailURL = id3v2['APIC'].imageUrl;
          else if (id3v2['PIC'])
            metadata.thumbnailURL = id3v2['PIC'].imageUrl;

          metadata.description = [];

          for (var key in id3v2) {
            if (typeof(Id3Parser.v2.MAPPERS[key]) != 'undefined' &&
                id3v2[key].value.trim().length > 0) {
              metadata.description.push({
                    key: Id3Parser.v2.MAPPERS[key],
                    value: id3v2[key].value.trim()
                  });
            }
          }

          metadata.description.sort(function(a, b) {
            return Id3Parser.METADATA_ORDER.indexOf(a.key)-
                   Id3Parser.METADATA_ORDER.indexOf(b.key);
          });
          this.nextStep();
        }
      ],
      this
  );

  var metadataParser = new FunctionParallel(
      'mp3metadataParser',
      [id3v1Parser, id3v2Parser],
      this,
      function() {
        callback.call(null, metadata);
      },
      onError
  );

  id3v1Parser.setCallback(metadataParser.nextStep);
  id3v2Parser.setCallback(metadataParser.nextStep);

  id3v1Parser.setFailureCallback(metadataParser.onError);
  id3v2Parser.setFailureCallback(metadataParser.onError);

  this.vlog('Passed argument : ' + file);

  metadataParser.start(file);
};


/**
 * Metadata order to use for metadata generation
 */
Id3Parser.METADATA_ORDER = [
  'ID3_TITLE',
  'ID3_LEAD_PERFORMER',
  'ID3_YEAR',
  'ID3_ALBUM',
  'ID3_TRACK_NUMBER',
  'ID3_BPM',
  'ID3_COMPOSER',
  'ID3_DATE',
  'ID3_PLAYLIST_DELAY',
  'ID3_LYRICIST',
  'ID3_FILE_TYPE',
  'ID3_TIME',
  'ID3_LENGTH',
  'ID3_FILE_OWNER',
  'ID3_BAND',
  'ID3_COPYRIGHT',
  'ID3_OFFICIAL_AUDIO_FILE_WEBPAGE',
  'ID3_OFFICIAL_ARTIST',
  'ID3_OFFICIAL_AUDIO_SOURCE_WEBPAGE',
  'ID3_PUBLISHERS_OFFICIAL_WEBPAGE'
];


/**
 * id3v1 constants
 */
Id3Parser.v1 = {
  /**
   * Genres list as described in id3 documentation. We aren't going to
   * localize this list, because at least in Russian (and I think most
   * other languages), translation exists at least fo 10% and most time
   * translation would degrade to transliteration.
   */
  GENRES : [
    'Blues',
    'Classic Rock',
    'Country',
    'Dance',
    'Disco',
    'Funk',
    'Grunge',
    'Hip-Hop',
    'Jazz',
    'Metal',
    'New Age',
    'Oldies',
    'Other',
    'Pop',
    'R&B',
    'Rap',
    'Reggae',
    'Rock',
    'Techno',
    'Industrial',
    'Alternative',
    'Ska',
    'Death Metal',
    'Pranks',
    'Soundtrack',
    'Euro-Techno',
    'Ambient',
    'Trip-Hop',
    'Vocal',
    'Jazz+Funk',
    'Fusion',
    'Trance',
    'Classical',
    'Instrumental',
    'Acid',
    'House',
    'Game',
    'Sound Clip',
    'Gospel',
    'Noise',
    'AlternRock',
    'Bass',
    'Soul',
    'Punk',
    'Space',
    'Meditative',
    'Instrumental Pop',
    'Instrumental Rock',
    'Ethnic',
    'Gothic',
    'Darkwave',
    'Techno-Industrial',
    'Electronic',
    'Pop-Folk',
    'Eurodance',
    'Dream',
    'Southern Rock',
    'Comedy',
    'Cult',
    'Gangsta',
    'Top 40',
    'Christian Rap',
    'Pop/Funk',
    'Jungle',
    'Native American',
    'Cabaret',
    'New Wave',
    'Psychadelic',
    'Rave',
    'Showtunes',
    'Trailer',
    'Lo-Fi',
    'Tribal',
    'Acid Punk',
    'Acid Jazz',
    'Polka',
    'Retro',
    'Musical',
    'Rock & Roll',
    'Hard Rock',
    'Folk',
    'Folk-Rock',
    'National Folk',
    'Swing',
    'Fast Fusion',
    'Bebob',
    'Latin',
    'Revival',
    'Celtic',
    'Bluegrass',
    'Avantgarde',
    'Gothic Rock',
    'Progressive Rock',
    'Psychedelic Rock',
    'Symphonic Rock',
    'Slow Rock',
    'Big Band',
    'Chorus',
    'Easy Listening',
    'Acoustic',
    'Humour',
    'Speech',
    'Chanson',
    'Opera',
    'Chamber Music',
    'Sonata',
    'Symphony',
    'Booty Bass',
    'Primus',
    'Porn Groove',
    'Satire',
    'Slow Jam',
    'Club',
    'Tango',
    'Samba',
    'Folklore',
    'Ballad',
    'Power Ballad',
    'Rhythmic Soul',
    'Freestyle',
    'Duet',
    'Punk Rock',
    'Drum Solo',
    'A capella',
    'Euro-House',
    'Dance Hall',
    'Goa',
    'Drum & Bass',
    'Club-House',
    'Hardcore',
    'Terror',
    'Indie',
    'BritPop',
    'Negerpunk',
    'Polsk Punk',
    'Beat',
    'Christian Gangsta Rap',
    'Heavy Metal',
    'Black Metal',
    'Crossover',
    'Contemporary Christian',
    'Christian Rock',
    'Merengue',
    'Salsa',
    'Thrash Metal',
    'Anime',
    'Jpop',
    'Synthpop'
  ]
};

/**
 * id3v2 constants
 */
Id3Parser.v2 = {
  FLAG_EXTENDED_HEADER: 1 << 5,

  ENCODING: {
    /**
     * ISO-8859-1 [ISO-8859-1]. Terminated with $00.
     *
     * @const
     * @type {int}
     */
    ISO_8859_1 : 0,


    /**
     * UTF-16 [UTF-16] encoded Unicode [UNICODE] with BOM. All
     * strings in the same frame SHALL have the same byteorder.
     * Terminated with $00 00.
     *
     * @const
     * @type {int}
     */
    UTF_16BE : 1,

    /**
     * UTF-16BE [UTF-16] encoded Unicode [UNICODE] without BOM.
     * Terminated with $00 00.
     *
     * @const
     * @type {int}
     */
    UTF_16 : 2,

    /**
     * UTF-8 [UTF-8] encoded Unicode [UNICODE]. Terminated with $00.
     *
     * @const
     * @type {int}
     */
    UTF_8 : 3
  },
  HANDLERS: {
   //User defined text information frame
   TXX: Id3Parser.prototype.readUserDefinedTextFrame_,
   //User defined URL link frame
   WXX: Id3Parser.prototype.readUserDefinedTextFrame_,

   //User defined text information frame
   TXXX: Id3Parser.prototype.readUserDefinedTextFrame_,

   //User defined URL link frame
   WXXX: Id3Parser.prototype.readUserDefinedTextFrame_,

   //User attached image
   PIC: Id3Parser.prototype.readPIC_,

   //User attached image
   APIC: Id3Parser.prototype.readAPIC_
  },
  MAPPERS: {
    TALB: 'ID3_ALBUM',
    TBPM: 'ID3_BPM',
    TCOM: 'ID3_COMPOSER',
    TDAT: 'ID3_DATE',
    TDLY: 'ID3_PLAYLIST_DELAY',
    TEXT: 'ID3_LYRICIST',
    TFLT: 'ID3_FILE_TYPE',
    TIME: 'ID3_TIME',
    TIT2: 'ID3_TITLE',
    TLEN: 'ID3_LENGTH',
    TOWN: 'ID3_FILE_OWNER',
    TPE1: 'ID3_LEAD_PERFORMER',
    TPE2: 'ID3_BAND',
    TRCK: 'ID3_TRACK_NUMBER',
    TYER: 'ID3_YEAR',
    WCOP: 'ID3_COPYRIGHT',
    WOAF: 'ID3_OFFICIAL_AUDIO_FILE_WEBPAGE',
    WOAR: 'ID3_OFFICIAL_ARTIST',
    WOAS: 'ID3_OFFICIAL_AUDIO_SOURCE_WEBPAGE',
    WPUB: 'ID3_PUBLISHERS_OFFICIAL_WEBPAGE'
  }
};

MetadataDispatcher.registerParserClass(Id3Parser);
