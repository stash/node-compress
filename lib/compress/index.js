var sys = require('sys');
var events = require('events');
var Buffer = require('buffer').Buffer;
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
function CommonStream() {
  events.EventEmitter.call(this);

  this.impl_ = null;

  this.dataQueue_ = [];
  this.paused_ = false;
  this.inputEncoding_ = null;
  this.outputEncoding_ = null;
}
sys.inherits(CommonStream, events.EventEmitter);


CommonStream.prototype.pause = function() {
  this.paused_ = true;
};


CommonStream.prototype.resume = function() {
  this.paused_ = false;
  this.emitData_();
};


CommonStream.prototype.destroy = function() {
  this.impl_.destroy();
};


CommonStream.prototype.write = function(buffer) {
  var self = this;

  if (this.inputEncoding_ != null) {
    // Not buffer input.
    var str = buffer;
    var len = Buffer.byteLength(str, this.inputEncoding_);
    buffer = new Buffer(len);
    if (len > 0) {
      buffer.write(str, this.inputEncoding_, 0);
    }
  }

  this.impl_.write(buffer, function(err, data) {
    self.emitEvent_(err, data);
  });
};


CommonStream.prototype.close = function() {
  var self = this;

  this.impl_.close(function(err, data) {
    self.emitEvent_(err, data, true);
  });
};


CommonStream.prototype.emitData_ = function() {
  if (!this.paused_) {
    for (var i = 0; i < this.dataQueue_.length; ++i) {
      var data = this.dataQueue_[i];
      if (data !== null) {
        this.emit('data', data);
      } else {
        this.emit('end');
      }
    }
    this.dataQueue_.length = 0;
  }
};

CommonStream.prototype.emitEvent_ = function(err, data, fin) {
  if (err) {
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


// === CompressStream ===
// Common base for compression streams.
function CompressStream() {
  CommonStream.call(this);
}
sys.inherits(CompressStream, CommonStream);


CompressStream.prototype.setEncoding = function(enc) {
  if (enc != 'binary') {
    throw new Error('CompressStream emits either Buffer or binary string.');
  }
  this.outputEncoding_ = 'binary';
};


CompressStream.prototype.setInputEncoding = function(enc) {
  if (enc == 'utf8' || enc == 'ascii') {
    this.inputEncoding_ = enc;
  } else {
    this.inputEncoding_ = 'binary';
  }
};


// === DecompressStream ===
// Common base for decompress streams.
function DecompressStream() {
  CommonStream.call(this);

}
sys.inherits(DecompressStream, CommonStream);


DecompressStream.prototype.setEncoding = function(enc) {
  if (enc == 'utf8' || enc == 'ascii') {
    this.outputEncoding_ = enc;
  } else {
    this.outputEncoding_ = 'binary';
  }
};


DecompressStream.prototype.setInputEncoding = function(enc) {
  if (enc != 'binary') {
    throw new Error('DecompressStream handles either Buffer or binary string.');
  }
  this.inputEncoding_ = 'binary';
};


// === GzipStream ===
function GzipStream() {
  CompressStream.call(this);

  this.impl_ = new Gzip();
}
sys.inherits(GzipStream, CompressStream);


// === GunzipStream ===
function GunzipStream() {
  DecompressStream.call(this);

  this.impl_ = new Gunzip();
}
sys.inherits(GunzipStream, DecompressStream);


// === BzipStream ===
function BzipStream() {
  CompressStream.call(this);

  this.impl_ = new Bzip();
}
sys.inherits(BzipStream, CompressStream);


// === BunzipStream ===
function BunzipStream() {
  DecompressStream.call(this);

  this.impl_ = new Bunzip();
}
sys.inherits(BunzipStream, DecompressStream);


exports.Gzip = Gzip;
exports.Gunzip = Gunzip;
exports.Bzip = Bzip;
exports.Bunzip = Bunzip;

exports.GzipStream = GzipStream;
exports.GunzipStream = GunzipStream;
exports.BzipStream = BzipStream;
exports.BunzipStream = BunzipStream;

