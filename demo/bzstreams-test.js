var compress=require("../compress");
var sys=require("sys");
var posix=require("fs");
var Buffer = require('buffer').Buffer;

sys.puts('Compressing...');
var bzdata = '';
var bzip=new compress.BzipStream();
bzip.setInputEncoding('utf8');
bzip.setEncoding('binary');
bzip.addListener('data', function(data) {
  bzdata += data;
}).addListener('error', function(err) {
  throw err;
}).addListener('end', function() {
  sys.puts('Compressed length: ' + bzdata.length);
});

bzip.write("My data that needs ");
bzip.write("to be compressed. 01234567890.");
bzip.close();
sys.puts('Compression finished.');

var d1 = bzdata.substr(0, 25);
var d2 = bzdata.substr(25);


sys.puts('Decompressing...');
var rawdata = '';
var bunzip = new compress.BunzipStream();
bunzip.setInputEncoding('binary');
bunzip.setEncoding('utf8');
bunzip.addListener('data', function(data) {
  rawdata += data;
}).addListener('error', function(err) {
  throw err;
}).addListener('end', function() {
  sys.puts('Decompressed length: ' + rawdata.length);
  sys.puts('Raw data: ' + rawdata);
});
bunzip.write(d1);
bunzip.write(d2);
bunzip.close();
sys.puts('Decompression finished.');

