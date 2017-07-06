
class Velocities:
  
  def __init__(self, vx, vy, vz):
    self.vx = vx
    self.vy = vy
    self.vz = vz

class Verticle:

  def __init__(self, x, y, area, t1):
    self.x = x
    self.y = y
    self.area = area
    self.t1 = t1

class ObjectTracker:

  def __init__(self, dequeLength=50):
    self.trackers = {'vx': VelocityTracker(self), 'vy': VelocityTracker(self), 'vz': VelocityTracker(self)}
    self.verticles = deque(maxlength=dequeLength)
    self.reset()

  def reset(self):
    self.lastContour = None
    self.resetTrackers()
  
  def resetTrackers(self):
    for tracker in self.trackers:
      tracker.reset()
    
  def update(self, contours, t1):
    if len(contours) == 0:
      self.resetTrackers()
      return
    
    contour = findContour(contours)
    if contour is None:
      self.resetTrackers()
      return
    
    self.verticles.appendleft(self.calculateVerticle(contour, t1))
    
    velocities = self.calculateVelocities(self.verticles[0], self.verticles[1])
    
    self.trackers['vx'].update(velocities.vx)
    self.trackers['vy'].update(velocities.vy)
    self.trackers['vz'].update(velocities.vz)
  
  def calculateVelocities(self, Verticle0, Verticle1):
    dx = Verticle1.x - Verticle0.x
    dy = Verticle1.y - Verticle0.y
    dz = calculateDistance()
    dt = (Verticle1.t1 - Verticle0.t1) / cv2.getTickCount()
    return Velocities(dx/dt, dy/dt, dz/dt)
    
  def fireEvent(self, eventName, tracker):
    # StartOfTracking
    # EndOfTracking
    # ResetTracker
  
  def calculateVerticle(self, contour, t1):
    M = cv2.moments(contour)
    x = M['01']/M['00']
    y = M['10']/M['00']
    area = cv2.area(contour)
    return Verticle(x, y, area, t1)

class VelocityTracker:	
	
	def __init__(self, parent, start_threshold=10, end_threshold=5):
		self.thresholds = {'start': start_threshold, 'end': end_threshold}
		self.reset()
		
	def reset(self):
		self.direction=0
		self.is_tracking_velocity=False
		self.max_velocity=0
		
	def update(self, velocity)
		direction = np.sign(velocity)
		abs_velocity = abs(velocity)
    
		if self.is_tracking_velocity:
      if self.direction != direction:
        # change in direction = end of tracking
        parent.fireEvent('EndOfTracking', self)
        self.reset()
        self.update(velocity)
        return
        
      if abs_velocity < self.thresholds['end']: 
        # velocity below threshold level = end of tracking
        parent.fireEvent('EndOfTracking', self)
        parent.fireEvent('ResetTracker', self)
        return
        
      else:
        # update max velocity
        # here self.direction == direction and
        # abs_velocity > self.velocity_threshold and
        # self.is_tracking_velocity == True
        self.max_velocity = max(self.max_velocity, abs_velocity)
        return
      
		else:
			# here we are not tracking velocity yet
      if abs_velocity > self.thresholds['start']:
        self.is_tracking_velocity = True
        self.max_velocity = abs_velocity
        self.direction=direction
        parent.fireEvent('StartOfTracking', self)
        return
