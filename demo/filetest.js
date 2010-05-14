var compress=require("../compress");
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


// Read in our test file
var data=posix.readFileSync("tests/filetest.js", encoding="binary");
sys.puts("Got: " + data.length);

// Set output file
var fd = posix.openSync("filetest.js.gz",
    process.O_WRONLY | process.O_TRUNC | process.O_CREAT, 0644);
sys.puts("Openned file");

// Create gzip stream
var gzip = new compress.Gzip();

// Pump data to be compressed
var gzdata, gzlast;
gzip.write(createBuffer(data, "binary"), function(err, data) {
  gzdata = data;
});
sys.puts("Compressed size: " + gzdata.length);
posix.writeSync(fd, gzdata, encoding="binary");

// Get the last bit

gzip.close(function(err, data) {
  gzlast = data;
});
sys.puts("Last bit: " + gzlast.length);
posix.writeSync(fd, gzlast, encoding="binary");
posix.closeSync(fd);
sys.puts("File closed");

// See if we can uncompress it ok
var source;
var gunzip = new compress.Gunzip();
var testdata = posix.readFileSync("filetest.js.gz", encoding="binary");
sys.puts("Test opened: " + testdata.length);
gunzip.write(createBuffer(testdata, "binary"), function(err, data) {
  source = data;
});
gunzip.close(function(err, data) {
  source += data;
});
sys.puts(source.length);
sys.puts(source);






