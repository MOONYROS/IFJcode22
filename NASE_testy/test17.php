<?php
declare(strict_types=1);
// FUNGUJE
$num = 4;
$num2 = 2;
function number1() : void{
   $num = 5;
   if($num === 5){
      number2();
   } else{
      $num = -5;
   }
}
function number2() : void{
   $num2 = -1;
   if($num2 === -1){
   $num2 = -3;
   $string1 = "Ah"."oj.";
   } else{
      $num2 = 0;
   }
}
if($num === 5){
   $num2 = 3;
}
else{
   number1();
}
?>