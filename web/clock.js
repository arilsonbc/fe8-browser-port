// Fixed simulation clock. A hidden tab pauses; it never fast-forwards the game.
(function(root){
 class FE8Clock {
  constructor(step){this.step=step;this.previous=null;this.pending=0;}
  reset(){this.previous=null;this.pending=0;}
  advance(now){
   if(this.previous===null){this.previous=now;return 0;}
   this.pending+=Math.max(0,Math.min(250,now-this.previous));this.previous=now;
   let count=0;
   while(this.pending+1e-8>=1000/60){this.pending-=1000/60;this.step();count++;}
   return count;
  }
 }
 if(typeof module==='object')module.exports=FE8Clock;
 else root.FE8Clock=FE8Clock;
})(globalThis);
