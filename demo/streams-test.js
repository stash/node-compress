var compress=require("../compress");
var sys=require("sys");
var posix=require("fs");
var Buffer = require('buffer').Buffer;

sys.puts('Compressing...');
var gzdata = '';
var gzip=new compress.GzipStream();
gzip.setInputEncoding('utf8');
gzip.setEncoding('binary');
gzip.addListener('data', function(data) {
  gzdata += data;
}).addListener('error', function(err) {
  throw err;
}).addListener('end', function() {
  sys.puts('Compressed length: ' + gzdata.length);
});

gzip.write("My data that needs ");
gzip.write("to be compressed. 01234567890.");
gzip.close();
sys.puts('Compression finished.');

var d1 = gzdata.substr(0, 25);
var d2 = gzdata.substr(25);


sys.puts('Decompressing...');
var rawdata = '';
var gunzip = new compress.GunzipStream();
gunzip.setInputEncoding('binary');
gunzip.setEncoding('utf8');
gunzip.addListener('data', function(data) {
  rawdata += data;
}).addListener('error', function(err) {
  throw err;
}).addListener('end', function() {
  sys.puts('Decompressed length: ' + rawdata.length);
  sys.puts('Raw data: ' + rawdata);
});
gunzip.write(d1);
gunzip.write(d2);
gunzip.close();
sys.puts('Decompression finished.');

