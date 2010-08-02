var events = require('events');
var Buffer = require('buffer').Buffer;
var assert = require('assert');
var bindings = require('./compress-bindings');

function removed(str) {
  return function() {
    throw new Error(str);
  }
}


function fallbackConstructor(str) {
  return function() {
    throw new Error(str);
  }
}


function inherits(ctor, superCtor) {
  ctor.prototype = Object.create(superCtor.prototype, {
      constructor: {
          value: ctor,
          enumerable: false
      }
  });
}


var Gzip = bindings.Gzip ||
           fallbackConstructor('Library built without gzip support.');
Gzip.prototype.init = removed('Use constructor to create new gzip object.');
Gzip.prototype.deflate = removed('Use write() instead.');
Gzip.prototype.end = removed('Use close() instead.');


var Gunzip = bindings.Gunzip ||
             fallbackConstructor('Library built without gzip support.');
Gunzip.prototype.init = removed('Use constructor to create new gunzip object.');
Gunzip.prototype.inflate = removed('Use write() instead.');
Gunzip.prototype.end = removed('Use close() instead.')


var Bzip = bindings.Bzip ||
           fallbackConstructor('Library built without bzip support.');
Bzip.prototype.init = removed('Use constructor to create new bzip object.');
Bzip.prototype.deflate = removed('Use write() instead.');
Bzip.prototype.end = removed('Use close() instead.');


var Bunzip = bindings.Bunzip ||
             fallbackConstructor('Library built without bzip support.');
Bunzip.prototype.init = removed('Use constructor to create new bunzip object.');
Bunzip.prototype.inflate = removed('Use write() instead.');
Bunzip.prototype.end = removed('Use close() instead.')


// === CommonStream ===
// Common base for compress/decompress streams.
function CommonStream(opt_noApiWarnings) {
  events.EventEmitter.call(this);

  this.impl_ = null;
  if (opt_noApiWarnings !== undefined) {
    this.apiWarnings_ = !opt_noApiWarnings;
  } else {
    this.apiWarnings_ = true;
  }

  this.dataQueue_ = [];
  this.paused_ = false;
  this.inputEncoding_ = null;
  this.outputEncoding_ = null;
  this.readable = true;
  this.writeable = true;
}
inherits(CommonStream, events.EventEmitter);


CommonStream.prototype.pause = function() {
  this.paused_ = true;
};


CommonStream.prototype.resume = function() {
  this.paused_ = false;
  this.emitData_();
};


CommonStream.prototype.destroy = function() {
  this.readable = false;
  this.writeable = false;
  this.impl_.destroy();
};


CommonStream.prototype.write = function(data, opt_encoding) {
  if (!this.writeable) {
    return true;
  }

  var self = this;
  var buffer = null;

  var encoding = null;
  if (!Buffer.isBuffer(data)) {
    encoding = opt_encoding || this.inputEncoding_ || 'utf8';
  }

  if (encoding !== null) {
    // Not buffer input.
    var len = Buffer.byteLength(data, encoding);
    if (len > 0) {
      buffer = new Buffer(len);
      buffer.write(data, encoding, 0);
    }
  } else {
    buffer = data;
  }

  if (Buffer.isBuffer(buffer)) {
    this.impl_.write(buffer, function(err, data) {
      self.emitEvent_(err, data);
    });
  } else {
    process.nextTick(function() {
      self.emitEvent_(Error('Fishy input'), null);
    });
  }
  return true;
};


CommonStream.prototype.end = function() {
  this.writeable = false;
  this.close();
};


CommonStream.prototype.close = function() {
  var self = this;

  this.impl_.close(function(err, data) {
    self.emitEvent_(err, data, true);
  });
};


CommonStream.prototype.setInputEncoding = function(enc) {
  this.apiWarning_('setInputEncoding() breaks standard streams API.\n' +
      '  The method is an extension to standard API and might be removed in ' +
      'future.');
  this.inputEncoding_ = enc;
};


CommonStream.prototype.emitData_ = function() {
  if (!this.paused_) {
    for (var i = 0; i < this.dataQueue_.length; ++i) {
      var data = this.dataQueue_[i];
      if (data !== null) {
        this.emit('data', data);
      } else {
        this.readable = false;
        this.emit('end');
      }
    }
    this.dataQueue_.length = 0;
  }
};

CommonStream.prototype.emitEvent_ = function(err, data, fin) {
  if (err) {
    this.readable = false;
    this.writeable = false;
    this.emit('error', err);
    return;
  }

  if (this.outputEncoding_ != 'binary') {
    var str = data;
    var len = Buffer.byteLength(str, 'binary');
    data = new Buffer(len);
    if (len > 0) {
      data.write(str, 'binary', 0); 
    }

    if (this.outputEncoding_ != null) {
      data = data.toString(this.outputEncoding_, 0, data.length);
    }
  }
  this.dataQueue_.push(data);

  if (fin) {
    this.dataQueue_.push(null);
  }
  this.emitData_();
};


CommonStream.prototype.apiWarning_ = function(str) {
  if (this.apiWarnings_) {
    console.warn(str);
  }
};


// === CompressStream ===
// Common base for compression streams.
function CompressStream(opt_noApiWarnings) {
  CommonStream.call(this, opt_noApiWarnings);
}
inherits(CompressStream, CommonStream);


CompressStream.prototype.setEncoding = function(enc) {
  assert.equal(enc, 'binary',
      'CompressStream emits either Buffer or binary string.');
  this.outputEncoding_ = 'binary';
};


// === DecompressStream ===
// Common base for decompress streams.
function DecompressStream(opt_noApiWarnings) {
  CommonStream.call(this, opt_noApiWarnings);
}
inherits(DecompressStream, CommonStream);


DecompressStream.prototype.setEncoding = function(enc) {
  this.outputEncoding_ = enc;
};


// === GzipStream ===
function GzipStream(opt_noApiWarnings) {
  CompressStream.call(this, opt_noApiWarnings);

  this.impl_ = new Gzip();
}
inherits(GzipStream, CompressStream);


// === GunzipStream ===
function GunzipStream(opt_noApiWarnings) {
  DecompressStream.call(this, opt_noApiWarnings);

  this.impl_ = new Gunzip();
}
inherits(GunzipStream, DecompressStream);


// === BzipStream ===
function BzipStream(opt_noApiWarnings) {
  CompressStream.call(this, opt_noApiWarnings);

  this.impl_ = new Bzip();
}
inherits(BzipStream, CompressStream);


// === BunzipStream ===
function BunzipStream(opt_noApiWarnings) {
  DecompressStream.call(this, opt_noApiWarnings);

  this.impl_ = new Bunzip();
}
inherits(BunzipStream, DecompressStream);


exports.Gzip = Gzip;
exports.Gunzip = Gunzip;
exports.Bzip = Bzip;
exports.Bunzip = Bunzip;

exports.GzipStream = GzipStream;
exports.GunzipStream = GunzipStream;
exports.BzipStream = BzipStream;
exports.BunzipStream = BunzipStream;

