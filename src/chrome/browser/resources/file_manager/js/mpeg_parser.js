// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function MpegParser(parent) {
  MetadataParser.apply(this, [parent]);
  this.verbose = true;
}

MpegParser.parserType = 'mpeg';

MpegParser.prototype = {__proto__: MetadataParser.prototype};

MpegParser.prototype.urlFilter = /\.(mp4|m4v|m4a|mpe?g4?)$/i;

MpegParser.HEADER_SIZE = 8;

MpegParser.readAtomSize = function(br, opt_end) {
  var pos = br.tell();

  if (opt_end) {
    // Assert that opt_end <= buffer end.
    // When supplied, opt_end is the end of the enclosing atom and is used to
    // check the correct nesting.
    br.validateRead(opt_end - pos);
  }

  var size = br.readScalar(4, false, opt_end);

  if (size < MpegParser.HEADER_SIZE)
    throw 'atom too short (' + size + ') @' + pos;

  if (opt_end && pos + size > opt_end)
    throw 'atom too long (' + size + '>' + (opt_end - pos)+ ') @' + pos;

  return size;
};

MpegParser.readAtomName = function(br, opt_end) {
  return br.readString(4, opt_end).toLowerCase();
};

MpegParser.createRootParser = function(metadata) {
  function findParentAtom(atom, name) {
    for (;;) {
      atom = atom.parent;
      if (!atom) return null;
      if (atom.name == name) return atom;
    }
  }

  function parseFtyp(br, atom) {
    metadata.brand = br.readString(4, atom.end);
  }

  function parseMvhd(br, atom) {
    var version = br.readScalar(4, false, atom.end);
    var offset = (version == 0) ? 8 : 16;
    br.seek(offset, ByteReader.SEEK_CUR);
    var timescale = br.readScalar(4, false, atom.end);
    var duration = br.readScalar(4, false, atom.end);
    metadata.duration =  duration / timescale;
  }

  function parseHdlr(br, atom) {
    br.seek(8, ByteReader.SEEK_CUR);
    findParentAtom(atom, 'trak').trackType = br.readString(4, atom.end);
  }

  function parseStsd(br, atom) {
    var track = findParentAtom(atom, 'trak');
    if (track && track.trackType == 'vide') {
      br.seek(40, ByteReader.SEEK_CUR);
      metadata.width = br.readScalar(2, false, atom.end);
      metadata.height = br.readScalar(2, false, atom.end);
    }
  }

  function parseDataString(name, br, atom) {
    br.seek(8, ByteReader.SEEK_CUR);
    metadata[name] = br.readString(atom.end - br.tell(), atom.end);
  }

  function parseCovr(br, atom) {
    br.seek(8, ByteReader.SEEK_CUR);
    metadata.thumbnailURL = br.readImage(atom.end - br.tell(), atom.end);
  }

  // 'meta' atom can occur at one of the several places in the file structure.
  var parseMeta = {
    ilst: {
      "©nam": { data : parseDataString.bind(null, "title") },
      "©alb": { data : parseDataString.bind(null, "album") },
      "©art": { data : parseDataString.bind(null, "artist") },
      "covr": { data : parseCovr }
    },
    versioned: true
  };

  // main parser for the entire file structure.
  return {
    ftyp:  parseFtyp,
    moov: {
      mvhd : parseMvhd,
      trak: {
        mdia: {
          hdlr: parseHdlr,
          minf: {
            stbl: {
              stsd: parseStsd
            }
          }
        },
        meta: parseMeta
      },
      udta: {
        meta: parseMeta
      },
      meta: parseMeta
    },
    meta: parseMeta
  };
};

MpegParser.prototype.parse = function (file, callback, onError) {
  var metadata = {metadataType: 'mpeg'};

  this.rootParser_ = MpegParser.createRootParser(metadata);

  // Kick off the processing by reading the first atom's header.
  this.requestRead(file, 0, MpegParser.HEADER_SIZE, null,
      onError, callback.bind(null, metadata));
};

MpegParser.prototype.applyParser = function(parser, br, atom, filePos) {
  if (this.verbose) {
    var path = atom.name;
    for (var p = atom.parent; p && p.name; p = p.parent) {
      path = p.name + '.' + path;
    }

    var action;
    if (!parser) {
      action = 'skipping ';
    } else if (parser instanceof Function) {
      action = 'parsing  ';
    } else {
      action = 'recursing';
    }

    var start = atom.start - MpegParser.HEADER_SIZE;
    this.vlog(path + ': ' +
              '@' + (filePos + start) + ':' + (atom.end - start),
              action);
  }

  if (parser) {
    if (parser instanceof Function) {
      br.pushSeek(atom.start);
      parser(br, atom);
      br.popSeek();
    } else {
      if (parser.versioned) {
        atom.start += 4;
      }
      this.parseMpegAtomsInRange(parser, br, atom, filePos);
    }
  }
};

MpegParser.prototype.parseMpegAtomsInRange = function(
    parser, br, parentAtom, filePos) {
  var count = 0;
  for (var offset = parentAtom.start; offset != parentAtom.end;) {
    if (count++ > 100) // Most likely we are looping through a corrupt file.
      throw "too many child atoms in " + parentAtom.name + " @" + offset;

    br.seek(offset);
    var size = MpegParser.readAtomSize(br, parentAtom.end);
    var name = MpegParser.readAtomName(br, parentAtom.end);

    this.applyParser(
        parser[name],
        br,
        { start: offset + MpegParser.HEADER_SIZE,
          end: offset + size,
          name: name,
          parent: parentAtom
        },
        filePos
    );

    offset += size;
  }
}

MpegParser.prototype.requestRead = function(
    file, filePos, size, name, onError, onSuccess) {
  var self = this;
  var reader = new FileReader();
  reader.onerror = onError;
  reader.onload = function(event) {
    self.processTopLevelAtom(
        reader.result, file, filePos, size, name, onError, onSuccess);
  };
  this.vlog("reading @" + filePos + ":" + size);
  reader.readAsArrayBuffer(file.webkitSlice(filePos, filePos + size));
}

MpegParser.prototype.processTopLevelAtom = function(
    buf, file, filePos, size, name, onError, onSuccess) {
  try {
    var br = new ByteReader(buf);

    // the header has already been read.
    var atomEnd = size - MpegParser.HEADER_SIZE;

    var bufLength = buf.byteLength;

    // Check the available data size. It should be either exactly
    // what we requested or HEADER_SIZE bytes less (for the last atom).
    if (bufLength != atomEnd && bufLength != size) {
      throw "Read failure @" + filePos + ", " +
          "requested " + size + ", read " + bufLength;
    }

    // Process the top level atom.
    if (name) { // name is null only the first time.
      this.applyParser(
          this.rootParser_[name],
          br,
          {start: 0, end: atomEnd, name: name},
          filePos
      );
    }

    filePos += bufLength;
    if (bufLength == size) {
      // The previous read returned everything we asked for, including
      // the next atom header at the end of the buffer.
      // Parse this header and schedule the next read.
      br.seek(-MpegParser.HEADER_SIZE, ByteReader.SEEK_END);
      var nextSize = MpegParser.readAtomSize(br);
      var nextName = MpegParser.readAtomName(br);

      // If we do not have a parser for the next atom, skip the content and
      // read only the header (the one after the next).
      if (!this.rootParser_[nextName]) {
        filePos += nextSize - MpegParser.HEADER_SIZE;
        nextSize = MpegParser.HEADER_SIZE;
      }

      this.requestRead(file, filePos, nextSize, nextName, onError, onSuccess);
    } else {
      // The previous read did not return the next atom header, EOF reached.
      this.vlog("EOF @" + filePos);
      onSuccess();
    }
  } catch(e) {
    return onError(e.toString());
  }
};

MetadataDispatcher.registerParserClass(MpegParser);
