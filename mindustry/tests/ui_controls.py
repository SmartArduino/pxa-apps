# SPDX-License-Identifier: GPL-3.0-only
"""Public touch gestures for the folded game HUD in rectangular test viewports."""
class Hud:
 def __init__(self,width,height,tap,bounds=None):
  self.w=width;self.h=height;self.tap=tap
  self.left,self.top,self.width,self.height=bounds or (0,0,width,height)
  self.right=self.left+self.width;self.bottom=self.top+self.height
  self.n=max(30,min(44,min(self.width,self.height)//10));self.cell=(self.width-20)//4
  self.mode='normal';self.cy=(self.top+4+self.n+self.bottom-4-self.n)//2
 def dock(self,i):self.tap(self.left+4+i*(self.cell+4)+self.cell//2,self.bottom-4-self.n//2)
 def cancel(self):self.dock(3);self.mode='normal'
 def choose(self,cat,index,count):
  self.dock(0);y=self.bottom-8-3*self.n-18
  self.tap(self.left+4+int((cat+.5)*(self.width-8)/7),y+self.n//2)
  self.tap(self.left+4+int((index+.5)*(self.width-8)/count),y+self.n+(self.n+18)//2)
  self.mode='build'
 def jump(self,x,y):
  if self.mode=='normal':self.dock(2)
  else:self.tap(self.right-4-self.n//2,self.top+4+self.n+6+self.n//2)
  scale=2 if self.n>=40 else 1
  self.tap(self.right-4-48*scale+x*scale,self.top+4+self.n+6+y*scale)
 def confirm(self):self.dock(2)
 def supply(self):self.dock(1)
 def tools(self,index):
  self.dock(3);top=self.bottom-8-self.n-(3*self.n+12)
  self.tap(self.left+8+(index%2)*(self.width-12)//2+(self.width-20)//4,top+4+(index//2)*(self.n+2)+self.n//2)
 def pause(self):self.tap(self.left+4+self.n//2,self.top+4+self.n//2)
 def wave(self):self.tap(self.left+4+self.n+32,self.top+4+self.n//2)

 def menu(self,i):self.tap(self.w//2,self.top+12+self.n+i*(self.n+7)+self.n//2)
 def back(self):self.pause()
 def confirm_new(self):self.tap(self.w*3//4,self.top+12+self.n+70+self.n//2)
