var compress=require("../compress-bindings");
var sys=require("sys");
var posix=require("fs");
var Buffer = require('buffer').Buffer;

function createBuffer(str, enc) {
  enc = enc || 'utf8';
  var len = Buffer.byteLength(str, enc);
  var buf = new Buffer(len);
  buf.write(str, enc, 0);
  return buf;
}


// Create gzip stream
var gzip=new compress.Gzip(4);
//gzip.init();

// Pump data to be compressed
var gzdata1, gzdata2, gzdata3;
gzip.write(createBuffer("My data that needs "), function(err, data) {
  gzdata1 = data;
});
sys.puts("Compressed size : "+gzdata1.length);

gzip.write(createBuffer("to be compressed. 01234567890."), function(err, data) {
  gzdata2 = data;
}); 
sys.puts("Compressed size : "+gzdata2.length);

gzip.close(function(err, data) {
  gzdata3 = data;
});
sys.puts("Last bit : "+gzdata3.length);

// Take the output stream, and chop it up into two
var gzdata = gzdata1+gzdata2+gzdata3;
sys.puts("Total compressed size : "+gzdata.length);
var d1 = gzdata.substr(0, 25);
var d2 = gzdata.substr(25);

// Create gunzip stream to decode these
var data1, data2, data3;
var gunzip = new compress.Gunzip;
gunzip.write(createBuffer(d1, 'binary'), function(err, data) {
  data1 = data;
});
gunzip.write(createBuffer(d2, 'binary'), function(err, data) {
  data2 = data;
});
gunzip.close(function(err, data) {
  data3 = data;
});

sys.puts(data1+data2+data3);






