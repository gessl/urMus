FreeAllRegions()
FreeAllFlowboxes()

DPrint("")

ball = {}
pegs = {}
harm = {}
dac = {}
osc = {}

size = 8 						--size of balls
pegsize = 6 					--size of pegs
MAXSPEED = 7 					--maximum speed
Npeg = 10 					--pegs in row
Mpeg = 7						--pegs in collumn
pegamount = Npeg*Mpeg 				--total pegs
amount = 20					--total balls
damp = 0.7 					--damping coefficient
time = 0 
soundtime = 1

local log = math.log    				--call math.log when log is used

--Set up flowboxes
dac = FBDac
sample = FlowBox(FBSample)
push_loop = FlowBox(FBPush)
push_pos = FlowBox(FBPush)
push_sound = FlowBox(FBPush)

--sample is an table of sound files
--order of sounds in sample is the same as order of images in self
--sample:AddFile(DocumentPath("rain.wav"))
sample:AddFile("rainsound.wav")

dac.In:SetPull(sample.Out)
push_loop.Out:SetPush(sample.Loop)
push_loop:Push(-1) --set looping off
push_pos.Out:SetPush(sample.Pos)
push_sound.Out:SetPush(sample.Sample)

push_pos:Push(1) --hack: don't play sound at start of program

--play sound file associated with the image corresponding to self.index
function playsound(self)
	push_pos:Push(0)
	push_sound:Push(0)
end

function UpdateBounce(self, elapsed)

    -- Balls Bumping into each other
    for x=1,amount do
    	other=ball[x]
        local dx=self.x-other.x
        local dy=self.y-other.y
        local d=math.sqrt(dx*dx+dy*dy);
        if d <= size then
        	local dvx=self.vx-other.vx
            local dvy=self.vy-other.vy
            if dvx*dx + dvy*dy < 0 then -- ball is moving toward each other
            	local fy=1.0e-10*math.abs(dy)
                local sign
                if math.abs(dx)<fy then
                	if dx<0 then
                    	sign=-1
                    else
                        sign=1
                    end
                dx=fy*sign
                end
                local a = dy/dx
                local dvx2=-(dvx + a*dvy)/(1+a*a)

                self.vx = self.vx + dvx2
                self.vy = self.vy + a*dvx2
                other.vx = other.vx - dvx2
                other.vy = other.vy - a*dvx2
		end
    	end

    end

	-- Balls bumping into pegs
	for i = 1,pegamount do
		--if a ball is inside a peg radius
		if (self.x - pegs[i].centerX)^2 + (self.y - pegs[i].centerY)^2 <= pegsize^2 then
	
	--call frequency corresponding to peg y index
			playsound(self)

			--vector.x  pointing from impact position to peg center
			local rx = -self.x + pegs[i].centerX
			
			--vector.y  pointing from impact position to peg center
			local ry = -self.y + pegs[i].centerY
	
			--magnitude of r, v
			local rmag = math.sqrt(rx^2+ry^2)
			local vmag = math.sqrt(self.vx^2+self.vy^2)

			--dot product r*v
			local dot = rx*self.vx + ry*self.vy

			--counter-clockwise surface tangent parameterization of x, y
			local tanx = -ry
			local tany = rx

			--angle of rotation (not signed)
			local angle = math.pi-2*math.acos(dot/(rmag*vmag))

			--if velocity hits peg at clockwise orientation
			if self.vx*tanx + self.vy*tany < 0 then 		     
				sign2 = -1 		       	     --flip sign of rotation (clockwise)
			else
				sign2 = 1 				     --flip sign of rotation (counterclockwise)
			end

--counterclockwise rotation of velocity by angle
			self.vx = damp*self.vx*math.cos(sign2*angle) - damp*self.vy*math.sin(sign2*angle)   
			self.vy = damp*self.vx*math.sin(sign2*angle) + damp*self.vy*math.cos(sign2*angle)

			--push ball one pixel away from peg
			self.x = self.x - rx/math.abs(rx)
			self.y = self.y - ry/math.abs(ry)
		end
	end
	time = time + elapsed    			--update time
	if time > soundtime then 			--if time > time cutoff
		time = time-soundtime 		--set time to the remainder
	end
	--position update
    self.x = self.x + self.vx
    self.y = self.y + self.vy
	--hitting the walls
    if (self.x < 0) then
        self.x=0
        self.vx = -damp*self.vx
    end
    if (self.y < 0) then
        self.y=ScreenHeight()-self.height
    end

    if(self.x > ScreenWidth()-self.width) then
        self.x = ScreenWidth()-self.width
        self.vx = -self.vx*damp
    end
    if(self.y > ScreenHeight()-self.height) then
        self.y = 0
    end
	--update visuals
    self:SetAnchor('BOTTOMLEFT', self.x , self.y)
    self.texture:SetSolidColor(15,255,100,255)
end


function UpdateBubble(self)
    self.x = self.x + self.vx
    self.y = self.y + self.vy
    self:SetAnchor('BOTTOMLEFT', self.x , self.y)
    r=23
    g=100
    b=135
    a=255
    self.texture:SetGradientColor("TOP", r, g, b, a, r, g, b, a)
end

function AccelerateBall(self,x,y,z)
    self.vx = self.vx + x
    self.vy = self.vy + y
	if self.vx >= MAXSPEED then
		self.vx = MAXSPEED
	end
	if self.vx <= -MAXSPEED then
		self.vx = -MAXSPEED
	end
	if self.vy >= MAXSPEED then
		self.vy = MAXSPEED
	end
	if self.vy <= -MAXSPEED then
		self.vy = -MAXSPEED
	end
end

backdrop = Region('Region','backdrop',UIParent)
backdrop:SetLayer("TOOLTIP")
backdrop:SetWidth(ScreenWidth());
backdrop:SetHeight(ScreenHeight());
backdrop:SetAnchor("BOTTOMLEFT",0,0)
backdrop:EnableInput(true)
backdrop.texture=backdrop:Texture()
backdrop.texture:SetTexture(0x11,0x22,0x44,0xff)
backdrop:Handle("OnUpdate",UpdateMic)
backdrop:Handle("OnDoubleTap",DoubleTap)
backdrop:Show()

-- MAKE PEGS

count = 0
for i = 0, Npeg do
	for j = 0, Mpeg do
		count = count+1
    		pegs[count] = Region()
    		pegs[count].indexY=j
    		pegs[count]:SetLayer("TOOLTIP")
 	   	pegs[count]:SetWidth(pegsize)
   	 	pegs[count]:SetHeight(pegsize)
    		pegs[count].width=pegsize
 	   	pegs[count].height=pegsize
		pegs[count].x = ScreenWidth()*(i+.5)/Npeg
		pegs[count].y = ScreenHeight()*(j+.5)/Mpeg
  	  	pegs[count].active=true
--		DPrint(ScreenHeight()*(j+.5)/Mpeg)
 	  	pegs[count]:Show()
		pegs[count]:EnableInput(true)
 	  	pegs[count]:SetAnchor("BOTTOMLEFT",pegs[count].x-pegsize/2,pegs[count].y - pegsize/2)
		pegs[count].centerX = pegs[count].x
		pegs[count].centerY = pegs[count].y
		pegs[count].texture = pegs[count]:Texture(255,0,0,255)
		pegs[count].texture:SetTexCoord(0.0,0.64,0.0,0.64)
		
		--pegs[count]:Handle("OnUpdate", play sounds)  MAKE ME
	end
end


-- MAKE BALLS

for k = 1,amount do
    ball[k] = Region('Region', 'ball', UIParent)
    ball[k].index=k
    ball[k]:SetLayer("TOOLTIP")
    ball[k]:SetWidth(size)
    ball[k]:SetHeight(size)
    ball[k].width=size
    ball[k].height=size
    ball[k].active=true
    ball[k]:Show()
    if k>1 then
        ball[k].prev=ball[k-1]
    end
    ball[k].x=math.random()*(ScreenWidth()-size)
    ball[k].y=math.random()*(ScreenHeight()-size)
    ball[k].vx=0--0math.random(-2,2)
    ball[k].vy=0--math.random(-2,2)
    ball[k]:SetAnchor("BOTTOMLEFT",ball[k].x,ball[k].y)
    ball[k].texture = ball[k]:Texture("small-ball.png")
    ball[k].texture:SetTexCoord(0.0,0.64,0.0,0.64)
    ball[k].texture:SetBlendMode("BLEND")
    ball[k]:Handle("OnUpdate",UpdateBounce)
    ball[k]:Handle("OnAccelerate",AccelerateBall)
end

local pagebutton=Region('region', 'pagebutton', UIParent)
pagebutton:SetWidth(pagersize)
pagebutton:SetHeight(pagersize)
pagebutton:SetLayer("TOOLTIP")
pagebutton:SetAnchor('BOTTOMLEFT',ScreenWidth()-pagersize-4,ScreenHeight()-pagersize-4)
pagebutton:EnableClamping(true)
pagebutton:Handle("OnTouchDown", MyFlipPage)
pagebutton.texture = pagebutton:Texture("circlebutton-16.png")
pagebutton.texture:SetGradientColor("TOP",255,255,255,255,255,255,255,255)
pagebutton.texture:SetGradientColor("BOTTOM",255,255,255,255,255,255,255,255)
pagebutton.texture:SetBlendMode("BLEND")
pagebutton.texture:SetTexCoord(0,1.0,0,1.0)
pagebutton:EnableInput(true)
pagebutton:Show()
