var sys = require('sys');
var events = require('events');
var bindings = require('compress-bindings');

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


CommonStream.prototype.emitData_ = function(data) {
  if (this.paused_) {
    this.dataQueue_.push(data);
  } else {
    for (var i = 0; i < this.dataQueue_.length; ++i) {
      self.emit('data', this.dataQueue_[i]);
    }
    self.dataQueue_.length = 0;
    if (data != null) {
      self.emit('data', data);
    }
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
    data.write(str, 'binary', 0); 

    if (this.outputEncoding_ != null) {
      data = data.toString(this.outputEncoding_, 0, data.length);
    }
  }
  this.emitData_(data);

  if (fin) {
    this.emit('end');
  }
};


// === CompressStream ===
// Common base for compression streams.
function CompressStream() {
  CommonStream.call(this);

  this.encoding_ = null;
}
sys.inherits(CompressStream, CommonStream);


CompressStream.prototype.setEncoding = function(enc) {
  if (enc != 'binary') {
    throw new Error('CompressStream emits either Buffer or binary string.');
  }
  this.outputString_ = true;
};


CompressStream.prototype.setInputEncoding = function(enc) {
  this.encoding_ = enc;
};


CompressStream.prototype.write = function(buffer, opt_encoding) {
  var self = this;

  opt_encoding = opt_encoding || this.encoding_;
  if (opt_encoding != null) {
    // Not buffer input.
    var str = buffer;
    buffer = new Buffer(Buffer.byteLength(str, opt_encoding));
    buffer.write(str, opt_encoding, 0);
  }

  this.impl_.write(buffer, function(err, data) {
    self.emitEvent_(err, data);
  });
};


CompressStream.prototype.close = function() {
  var self = this;

  this.impl_.close(function (err, data) {
    self.emitEvent_(err, data, true);
  });
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


DecompressStream.prototype.write = function(buffer) {
  var self = this;

  this.impl_.write(buffer, function(err, data) {
    self.emitEvent_(err, data);
  });
};


DecompressStream.prototype.close = function() {
  var self = this;

  this.impl_.close(function (err, data) {
    self.emitEvent_(err, data, true);
  });
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

  this.impl_ = new GunzipStream();
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

  this.impl_ = new BunzipStream();
}
sys.inherits(BunzipStream, DecompressStream);


