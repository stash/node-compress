var compress=require("./compress");
var sys=require("sys");
var posix=require("fs");

// Create bzip stream
var bzip=new compress.Bzip;
bzip.init();

// Pump data to be compressed
var gzdata1 = bzip.deflate("My data that needs ", "binary"); 
sys.puts("Compressed size : "+gzdata1.length);

var gzdata2 = bzip.deflate("to be compressed. 01234567890.", "binary"); 
sys.puts("Compressed size : "+gzdata2.length);

var gzdata3=bzip.end();
sys.puts("Last bit : "+gzdata3.length);

// Take the output stream, and chop it up into two
var gzdata = gzdata1+gzdata2+gzdata3;
sys.puts("Total compressed size : "+gzdata.length);
var d1 = gzdata.substr(0, 25);
var d2 = gzdata.substr(25);

// Create bunzip stream to decode these
var bunzip = new compress.Bunzip;
bunzip.init();
var data1 = bunzip.inflate(d1, "binary");
var data2 = bunzip.inflate(d2, "binary");
var data3 = bunzip.end();

sys.puts(data1+data2+data3);






