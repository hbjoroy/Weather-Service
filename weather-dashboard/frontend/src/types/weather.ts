export type TemperatureUnit = 'celsius' | 'fahrenheit'
export type WindUnit = 'kmh' | 'knots' | 'ms'

export interface UserProfile {
  userId: string
  name: string
  isAuthenticated: boolean
  tempUnit: TemperatureUnit
  windUnit: WindUnit
  defaultLocation: string
}

export interface LoginRequest {
  userId: string
  name: string
}

export interface LoginResponse {
  success: boolean
  userId: string
  name: string
  sessionId: string
}

export interface WeatherCondition {
  text: string
  icon: string
  code: number
}

export interface Location {
  name: string
  region: string
  country: string
  lat: number
  lon: number
  tz_id: string
  localtime_epoch: number
  localtime: string
}

export interface CurrentWeather {
  last_updated_epoch: number
  last_updated: string
  temp_c: number
  temp_f: number
  is_day: number
  condition: WeatherCondition
  wind_mph: number
  wind_kph: number
  wind_degree: number
  wind_dir: string
  pressure_mb: number
  pressure_in: number
  precip_mm: number
  precip_in: number
  humidity: number
  cloud: number
  feelslike_c: number
  feelslike_f: number
  vis_km: number
  vis_miles: number
  uv: number
  gust_mph: number
  gust_kph: number
}

export interface WeatherResponse {
  location: Location
  current: CurrentWeather
}

export interface Astronomy {
  sunrise: string
  sunset: string
  moonrise: string
  moonset: string
  moon_phase: string
  moon_illumination: number
}

export interface ForecastDay {
  maxtemp_c: number
  maxtemp_f: number
  mintemp_c: number
  mintemp_f: number
  avgtemp_c: number
  avgtemp_f: number
  maxwind_mph: number
  maxwind_kph: number
  totalprecip_mm: number
  totalprecip_in: number
  avghumidity: number
  daily_will_it_rain: number
  daily_chance_of_rain: number
  uv: number
  condition: WeatherCondition
}

export interface ForecastHour {
  time_epoch: number
  time: string
  temp_c: number
  temp_f: number
  is_day: number
  condition: WeatherCondition
  wind_mph: number
  wind_kph: number
  wind_degree: number
  wind_dir: string
  humidity: number
  cloud: number
  precip_mm: number
  chance_of_rain: number
}

export interface ForecastDaily {
  date: string
  date_epoch: number
  day: ForecastDay
  astro: Astronomy
  hour?: ForecastHour[]
}

export interface ForecastResponse {
  location: Location
  forecast: {
    forecastday: ForecastDaily[]
  }
}

export interface ForecastRequest {
  location: string
  days: number
  include_aqi?: boolean
  include_alerts?: boolean
  include_hourly?: boolean
}

export interface ApiError {
  code: number
  message: string
  details?: string
}

export interface ErrorResponse {
  error: ApiError
}